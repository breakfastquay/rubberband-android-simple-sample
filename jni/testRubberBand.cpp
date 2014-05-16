
#include "rubberband/RubberBandStretcher.h"

#include <string.h>
#include <jni.h>
#include <time.h>
#include <cpu-features.h>

#include <string>
#include "rubberband/src/dsp/FFT.h"
#include "rubberband/src/base/Profiler.h"

#include <android/log.h>
#define D(x...) __android_log_print(ANDROID_LOG_INFO, "TestRubberBand", x)

extern "C" {
jstring
Java_com_breakfastquay_rubberbandexample_RubberBandExampleActivity_testRubberBand(JNIEnv*, jobject);
}

using namespace RubberBand;

static void printLong(std::string s)
{
    std::string bit;
    int j = 0;
    for (int i = 0; i < s.length(); ++i) {
	if (s[i] == '\n') {
	    if (i > j) {
		D("%s", bit.c_str());
		bit = "";
	    }
	    j = i + 1;
	} else {
	    bit += s[i];
	}
    }
    if (bit != "") D("%s", bit.c_str());
}

jstring
Java_com_breakfastquay_rubberbandexample_RubberBandExampleActivity_testRubberBand(JNIEnv* env, jobject thiz)
{
    char message[200];
    
    uint64_t features = android_getCpuFeatures();
    D("CPU features: %d", (int)features);
    if ((features & ANDROID_CPU_ARM_FEATURE_ARMv7) == 0) {
        return env->NewStringUTF("Not an ARMv7 CPU\n");
    }
    if ((features & ANDROID_CPU_ARM_FEATURE_NEON) == 0) {
	return env->NewStringUTF("CPU lacks NEON support\n");
    }

#ifdef FFT_MEASUREMENT
    D("Running FFT tune...\n");
    std::string fftreport = FFT::tune();
    D("Report follows:\n");
    printLong(fftreport);
//    return env->NewStringUTF(fftreport.c_str());
#endif

    struct timeval begin;
    gettimeofday(&begin, 0);

#define SECONDS 30
#define RATE 44100
#define RATIO 1.01
#define PITCHSHIFT 1.0
#define SAMPLES (SECONDS * RATE)

    D("Ratio %lf, pitch shift %lf, total input samples: %d\n", RATIO, PITCHSHIFT, SAMPLES);

    float *input = new float[SAMPLES];

    int insamples = SAMPLES;
    int outsamples = insamples * RATIO;
    
    float irms = 0, orms = 0;

    for (int i = 0; i < insamples; ++i) {
	input[i] = sinf(float(i) / 100.f);
    }

    for (int i = 0; i < insamples; ++i) {
//	input[i] = float(i % 100) / 50.f - 1.f;
	irms += input[i] * input[i];
    }
    irms = sqrtf(irms / insamples);

    RubberBandStretcher ts(RATE, 1,
			   RubberBandStretcher::OptionProcessRealTime |
			   RubberBandStretcher::OptionWindowShort,
			   RATIO, PITCHSHIFT);

    int outspace = outsamples + 44100;
    float *output = new float[outspace];

    if (!output) {
	D("Failed to allocate space for %d samples\n", outspace);
	return env->NewStringUTF("Allocation failed");
    }

    ts.setExpectedInputDuration(insamples);

    int iin = 0, iout = 0;
    int bs = 1024;
    int avail = 0;

    D("Total output samples: %d\n", outsamples);
/*
    while (iin < SAMPLES) {

	int thisblock = SAMPLES - iin;
	if (thisblock > bs) thisblock = bs;
	float *iptr = input + iin;
	ts.study(&iptr, thisblock, (iin + thisblock == SAMPLES));
	iin += thisblock;
	
	D("Studied: %d\n", iin);
    }

    iin = 0;
*/
    
    int printcounter = 0;

    while (iin < SAMPLES) {

	int thisblock = SAMPLES - iin;
	if (thisblock > bs) thisblock = bs;
	float *iptr = input + iin;
	ts.process(&iptr, thisblock, (iin + thisblock == SAMPLES));
	iin += thisblock;

	if ((avail = ts.available()) > 0) {
	    int thisout = avail;
	    if (iout + thisout > outspace) thisout = outspace - iout;
	    float *optr = output + iout;
	    ts.retrieve(&optr, thisout);
	    for (int i = 0; i < thisout; ++i) {
		orms += optr[i] * optr[i];
	    }
	    iout += thisout;
	}

	if (++printcounter == 10) {
	    D("Processed: %d\n", iout);
	    printcounter = 0;
	}
    }

    while ((avail = ts.available()) > 0) {
	D("Available: %d\n", avail);
	int thisout = avail;
	if (iout + thisout > outspace) {
	    D("iout = %d, thisout = %d, but outspace is only %d\n", iout, thisout, outspace);
	    thisout = outspace - iout;
	}
	float *optr = output + iout;
	ts.retrieve(&optr, thisout);
	for (int i = 0; i < thisout; ++i) {
	    orms += optr[i] * optr[i];
	}
	iout += thisout;
	D("Processed: %d\n", iout);
    }

    D("Done, processed: %d\n", iout);
    orms = sqrtf(orms / iout);

    struct timeval end;
    gettimeofday(&end, 0);

    int secs = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) --secs;

    D(message, "iin = %d, iout = %d, in rms = %f, out rms = %f, elapsed = %d, in fps = %d, out fps = %d",
      iin, iout, irms, orms, secs, iin / secs, iout / secs);

    sprintf(message, "iin = %d, iout = %d, in rms = %f, out rms = %f, elapsed = %d, in fps = %d, out fps = %d",
	    iin, iout, irms, orms, secs, iin / secs, iout / secs);

    D("...");
    D("0.2 sec from input:");
    for (int i = 44100 * 10; i < 44100 * 10 + (44100 / 5); i += 4) {
	D("%f %f %f %f", input[i], input[i+1], input[i+2], input[i+3]);
    }

    D("...");
    D("0.2 sec from output:");
    for (int i = 44100 * 10; i < 44100 * 10 + (44100 / 5); i += 4) {
	D("%f %f %f %f", output[i], output[i+1], output[i+2], output[i+3]);
    }

    delete[] input;
    delete[] output;

#ifdef WANT_TIMING
    std::string report = Profiler::getReport();
    D("Done, report follows (%d chars):\n", report.length());
    printLong(report);
#endif

    return env->NewStringUTF(message);
}
