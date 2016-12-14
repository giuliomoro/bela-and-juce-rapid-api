/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic startup code for a Juce application.

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"

#include <Bela.h>
#include <Midi.h>
#include <OscillatorBank.h>
OscillatorBank osc;
void randomize();
void recalculate_frequencies();
int gState;

enum {
	kCollecting,
	kProcessing,
	kTraining,
	kIdle
};

    //==========================================================================
	/*** MACHINE LEARNING ***/

	// Rapid regression
Regression                      regressionObject;
Array <Regression::DataSample>  trainingSet;

Array <double> current;
//TouchSurface block;

void readKeyboard();
AuxiliaryTask gReadKeyboardTask;

void train()
{
	if (trainingSet.size() > 2) 
	{
		regressionObject.train(trainingSet);                
        printf("Trained\n");
	}
	else
	{
		fprintf(stderr, "Please record more audio");
	}
}

void addExample(Array <double> input, Array <double> output)
{
	Regression::DataSample example;
	example.inputs.addArray(input);
	example.outputs.addArray(output);
	
	trainingSet.add(example);
}

void process(Array <double> input)
{
	if (regressionObject.hasBeenTrained() && trainingSet.size() > 2)
	{
		Array <double> output = regressionObject.process(input);
        
        for(int n = 0; n < output.size(); ++n){
			current.set(n, output.getUnchecked(n));
		}
	}
	
}

void dumpCurrent(){
	printf("current: \n");
	for(int n = 0; n < current.size(); ++n){
		printf("[%d]:%6.3f  ", current[n]);
		if(n > 0 && n % 4 == 0){
			printf("\n");
		}
	}
	printf("\n");
}

const char* gMidiPort0 = "hw:1,0,0";
void midiMessageCallback(MidiChannelMessage message, void* arg){
	static int x = 0;
	static int y = 0;
	static int z = 0;
	if(message.getType() == kmmNoteOn){
		if(gState != kProcessing)
			message.prettyPrint();
		if(message.getDataByte(1) > 0){// note on
			int note = message.getDataByte(0) - 48;
			x = note % 5;
			y = note / 5;
		}
	}
	if(message.getType() == kmmChannelPressure){
		if(gState != kProcessing)
			message.prettyPrint();
		if(message.getDataByte(1) > 0){// note on
			z  = message.getDataByte(0);
		}
	}
	Array <double> input;
	input.add(x);
	input.add(y);
	input.add(z);
	switch(gState){
	case kCollecting:
		addExample(input, current);
		rt_printf("added example: x: %d, y: %d, z: %d\n", x, y, z);
		rt_printf("parameters: %.0f, %.0f, %.2f\n", current[0], current[1], current[2]);
	break;
	case kProcessing:
		process(input);
		recalculate_frequencies();
		rt_printf("processed input: x: %d, y: %d, z: %d\n", x, y, z);
		rt_printf("parameters: %.0f, %.0f, %.2f\n", current[0], current[1], current[2]);
	break;
	}
}


void readKeyboard()
{
	// This is not a real-time task because
	// select() and scanf() are system calls, not handled by Xenomai.
	// This task will be automatically down graded to "secondary mode"
	// the first time it is executed.

	char keyStroke = '.';

	fd_set readfds;
    struct timeval tv;
    int fd_stdin;
	fd_stdin = fileno(stdin);
	printf("fileno: %d\n", fd_stdin);
	while (!gShouldStop){
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		fflush(stdout);
		// Check if there are any characters ready to be read
		int num_readable = select(fd_stdin + 1, &readfds, NULL, NULL, &tv);
		// if there are, then read them
		if(num_readable > 0){
			scanf("%c", &keyStroke);
			printf("keyStroke detected: %c\n", keyStroke);
			if(keyStroke != '\n'){ // filter out the "\n" (newline) character
				switch (keyStroke)
				{
					case 'r':
						gState = kIdle;
						randomize();
						rt_printf("parameters: %.0f, %.0f, %.2f\n", current[0], current[1], current[2]);
					break;
					case 't':
						printf("Training\n");
						gState = kTraining;
						train();
						printf("Done training\n");
						gState = kProcessing;
						printf("Processing\n");
					break;
					case 'p':
						printf("Processing\n");
						gState = kProcessing;
					break;
					case 'c':
						printf("Collecting\n");
						gState = kCollecting;
					break;
					default:
						break;
				}

			}
		}
		usleep(1000);
	}
}






const float kMinimumFrequency = 20.0f;
const float kMaximumFrequency = 8000.0f;
// These settings are carried over from main.cpp
// Setting global variables is an alternative approach
// to passing a structure to userData in setup()
int gNumOscillators = 100;
int gWavetableLength = 1024;

Midi midi;

void recalculate_frequencies()
{
	float newMinFrequency = current[0];
	float newMaxFrequency = current[1];
	float balance = current[2];
	float freq = newMinFrequency;
	float increment = (newMaxFrequency - newMinFrequency) / (float)gNumOscillators;

	for(int n = 0; n < gNumOscillators; n++) {
		// Update the frequencies to a regular spread, plus a small amount of randomness
		// to avoid weird phase effects
		float randScale = 0.99 + .02 * (float)random() / (float)RAND_MAX;
		float newFreq = freq * randScale;

		osc.setFrequency(n, newFreq);
		freq += increment;
	}
}

void randomize(){
	current.set(0, (float)random() /(float)RAND_MAX * 1010 + 100);
	current.set(1, (float)random() /(float)RAND_MAX * 3010 + 1500);
	current.set(2, (float)random() /(float)RAND_MAX);
}

bool setup(BelaContext* context, void*)
{
	osc.init(gWavetableLength, gNumOscillators, context->audioSampleRate);
	
	// Fill in the wavetable with one period of your waveform
	float* wavetable = osc.getWavetable();
	for(int n = 0; n < osc.getWavetableLength() + 1; n++){
		wavetable[n] = sinf(2.0 * M_PI * (float)n / (float)osc.getWavetableLength());
	}
	
	// Initialise frequency and amplitude
	float freq = kMinimumFrequency;
	float increment = (kMaximumFrequency - kMinimumFrequency) / (float)gNumOscillators;
	for(int n = 0; n < gNumOscillators; n++) {
		if(context->analogFrames == 0) {
			// Random frequencies when used without analogInputs
			osc.setFrequency(n, kMinimumFrequency + (kMaximumFrequency - kMinimumFrequency) * ((float)random() / (float)RAND_MAX));
		}
		else {
			// Constant spread of frequencies when used with analogInputs
			osc.setFrequency(n, freq);
			freq += increment;
		}
		osc.setAmplitude(n, (float)random() / (float)RAND_MAX / (float)gNumOscillators);
	}

	increment = 0;
	freq = 440.0;

	for(int n = 0; n < gNumOscillators; n++) {
		// Update the frequencies to a regular spread, plus a small amount of randomness
		// to avoid weird phase effects
		float randScale = 0.99 + .02 * (float)random() / (float)RAND_MAX;
		float newFreq = freq * randScale;

		// For efficiency, frequency is expressed in change in wavetable position per sample, not Hz or radians
		osc.setFrequency(n, newFreq);
		freq += increment;
	}

	current.add((float)random() / (float)RAND_MAX * 200 + 200);
	current.add((float)random() / (float)RAND_MAX * 200 + 2000);
	current.add((float)random() / (float)RAND_MAX);

	gState = kIdle;
	printf("Collecting\n");
	midi.readFrom(gMidiPort0);
	midi.enableParser(true);
	midi.setParserCallback(midiMessageCallback, (void*) gMidiPort0);

	// Start the lower-priority task. It will run forever in a loop
	if((gReadKeyboardTask = Bela_createAuxiliaryTask(&readKeyboard, 50, "bela-trigger-samples")) == 0)
		return false;
	Bela_scheduleAuxiliaryTask(gReadKeyboardTask);
	
    return true;
}

void render(BelaContext* context, void*){
	float arr[context->audioFrames];
	// Render audio frames
	osc.process(context->audioFrames, arr);
	float balance = current.getUnchecked(2);
	for(unsigned int n = 0; n < context->audioFrames; ++n){
		float noise = random()/(float)RAND_MAX * 2.f - 1.f;
		float out = arr[n] * balance + (1.f - balance);
		audioWrite(context, n, 0, out);
		audioWrite(context, n, 1, out);
	}
}

void cleanup(BelaContext*, void*)
{

}
