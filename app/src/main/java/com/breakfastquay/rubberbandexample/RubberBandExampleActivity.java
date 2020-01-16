package com.breakfastquay.rubberbandexample;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;
import android.os.Bundle;
import android.media.AudioTrack;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.content.res.Resources;
import android.util.Log;
import android.util.TimingLogger;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;

import java.util.List;
import java.util.LinkedList;
import java.util.Collections;

import java.nio.ByteBuffer;

import com.breakfastquay.rubberband.RubberBandStretcher;

public class RubberBandExampleActivity extends Activity {
	/**
	 * Called when the activity is first created.
	 */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		int rate = 44100;

		m_audioTrackBufferSize =
				AudioTrack.getMinBufferSize(rate,
						AudioFormat.CHANNEL_OUT_STEREO,
						AudioFormat.ENCODING_PCM_16BIT);

		m_audioTrackBufferSize *= 4;

		m_track = new AudioTrack
				(AudioManager.STREAM_MUSIC,
						rate,
						AudioFormat.CHANNEL_OUT_STEREO,
						AudioFormat.ENCODING_PCM_16BIT,
						m_audioTrackBufferSize,
						AudioTrack.MODE_STREAM);

		m_buflist = Collections.synchronizedList(new LinkedList<short[]>());

		m_playThread = new Thread(m_playback);
		m_playThread.start();

		m_popThread = new Thread(m_populate);
		m_popThread.start();

		setContentView(R.layout.main);
	}

	List<short[]> m_buflist;
	AudioTrack m_track;
	int m_audioTrackBufferSize;
	Thread m_playThread;
	Thread m_popThread;
	boolean m_cancelled = false;

	Runnable m_playback = new Runnable() {
		public void run() {
			Thread.currentThread().setPriority(Thread.MIN_PRIORITY);

			while (true) {
				try {
					synchronized (m_buflist) {
						while (m_buflist.isEmpty() && !m_cancelled) {
							m_buflist.wait();
						}
					}
				} catch (java.lang.InterruptedException e) {
					Log.d("RubberBand", "run(): wait interrupted");
					continue;
				}
				if (m_cancelled) break;
				int size = m_buflist.size();
				Log.d(
						"RubberBand",
						size + " " + ((size == 1) ? "element" : "elements") + " in buflist"
				);
				short[] buf = m_buflist.get(0);
				m_buflist.remove(0);
				m_track.write(buf, 0, buf.length);
			}
		}
	};

	Runnable m_populate = new Runnable() {
		public void run() {
			try {
				int rate = 44100;
				double ratio = 3.5;
				double pitchshift = 2;

				InputStream audio = getResources().openRawResource(R.raw.a);
				ByteArrayOutputStream ostr = new ByteArrayOutputStream(audio.available());
				byte[] buf = new byte[1024];
				int r;
				while ((r = audio.read(buf)) >= 0) ostr.write(buf, 0, r);
				byte[] raw = ostr.toByteArray();

				long start = System.currentTimeMillis();

				// We assume raw is interleaved 2-channel 16-bit PCM
				int channels = 2;
				int frames = raw.length / (channels * 2);

				int insamples = frames;

				RubberBandStretcher ts = new RubberBandStretcher
						(rate, channels,
								RubberBandStretcher.OptionProcessRealTime +
										RubberBandStretcher.OptionPitchHighSpeed +
										RubberBandStretcher.OptionWindowLong,
								ratio, pitchshift);

				ts.setExpectedInputDuration(insamples);

				int iin = 0, iout = 0;
				int bs = 1024;
				int avail = 0;

				float[][] inblocks = new float[channels][bs];

				ByteBuffer bb = ByteBuffer.wrap(raw).order(null);

				int ppc = 0;

				int audioTrackBufferFrames = m_audioTrackBufferSize / 4;
				float[][] outblocks = new float[channels][audioTrackBufferFrames];

				Log.d("RubberBand", "audioTrackBufferFrames = " + audioTrackBufferFrames);

				// don't stack up more than 2 sec in buffers
				int maxBufs = rate / bs;
				int maxBufsSleep = 1000; // ms

				while (iin < insamples) {

					if (m_buflist.size() > maxBufs) {
						try {
							Log.d("RubberBand", "reached " + maxBufs + " bufs, sleeping " + maxBufsSleep);
							Thread.sleep(maxBufsSleep);
						} catch (java.lang.InterruptedException e) {
						}
					}

					int pc = (100 * iin) / insamples;
					if (pc != ppc) {
						Log.d("RubberBand", iin + " of " + insamples + " [" + pc + "%]");
						ppc = pc;
					}

					int thisblock = insamples - iin;
					if (thisblock > bs) thisblock = bs;

					for (int i = 0; i < thisblock; ++i) {
						for (int c = 0; c < channels; ++c) {
							short val = bb.getShort();
							inblocks[c][i] = (float) val / 32768;
						}
					}

					ts.process(inblocks, (iin + thisblock == insamples));
					iin += thisblock;

					while ((avail = ts.available()) > 0) {

						int retrieved = ts.retrieve(outblocks);
						short[] outbuf = new short[retrieved * channels];
						for (int i = 0; i < retrieved; ++i) {
							for (int c = 0; c < channels; ++c) {
								float f = outblocks[c][i];
								if (f < -1.0f) f = -1.0f;
								if (f > 1.0f) f = 1.0f;
								int ox = i * channels + c;
								outbuf[ox] = (short) (f * 32767);
							}
						}
						synchronized (m_buflist) {
							m_buflist.add(outbuf);
							m_buflist.notify();
						}
						iout += retrieved;
					}
				}

				ts.dispose();

				long end = System.currentTimeMillis();
				double t = (double) (end - start) / 1000;
				Log.d("RubberBand",
						"iin = " + iin + ", iout = " + iout + ", time = "
								+ t + ", in fps = " + iin / t + ", out fps = " + iout / t);

			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	};

	public void play(View v) {
		Log.d("RubberBand", "play");
		m_track.play();
	}

	public void stop(View v) {
		Log.d("RubberBand", "stop");
		m_track.flush();
		m_track.stop();
	}
}
