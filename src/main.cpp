

#include "MidiFile.h"
#include "Options.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace smf;

// user interface variables
Options options;
int     tpq = 480;              // ticks per quarter note
double  tempo = 120.0;          // time units will be in seconds
int     channel = 0;            // default channel

// user interface variables
double  tempo = 60.0;

// function declarations:
void      convertMidiFileToText (MidiFile& midifile);
void      setTempo              (MidiFile& midifile, int index, double& tempo);
void      checkOptions          (Options& opts, int argc, char** argv);
void      example               (void);
void      usage                 (const char* command);

//////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
   checkOptions(options, argc, argv);

   MidiFile midifile(options.getArg(1));
   convertMidiFileToText(midifile);





   fstream textfile(options.getArg(1).c_str(), ios::in);
   if (!textfile.is_open()) {
      cout << "Error: cannot read input text file." << endl;
      usage(options.getCommand().c_str());
      exit(1);
   }
   MidiFile midifile;
   convertTextToMidiFile(textfile, midifile);


   midifile.sortTracks();
   midifile.write(options.getArg(2));





   
   return 0;
}

//////////////////////////////////////////////////////////////////////////


//////////////////////////////
//
// convertMidiFileToText --
//

void convertMidiFileToText(MidiFile& midifile) {
   midifile.absoluteTicks();
   midifile.joinTracks();

   vector<double> ontimes(128);
   vector<int> onvelocities(128);
   int i;
   for (i=0; i<128; i++) {
      ontimes[i] = -1.0;
      onvelocities[i] = -1;
   }

   double offtime = 0.0;

   int key = 0;
   int vel = 0;

   for (i=0; i<midifile.getNumEvents(0); i++) {
      int command = midifile[0][i][0] & 0xf0;
      if (command == 0x90 && midifile[0][i][2] != 0) {
         // store note-on velocity and time
         key = midifile[0][i][1];
         vel = midifile[0][i][2];
         ontimes[key] = midifile[0][i].tick * 60.0 / tempo /
               midifile.getTicksPerQuarterNote();
         onvelocities[key] = vel;
      } else if (command == 0x90 || command == 0x80) {
         // note off command write to output
         key = midifile[0][i][1];
         offtime = midifile[0][i].tick * 60.0 /
               midifile.getTicksPerQuarterNote() / tempo;
         cout << "note\t" << ontimes[key]
              << "\t" << offtime - ontimes[key]
              << "\t" << key << "\t" << onvelocities[key] << endl;
         onvelocities[key] = -1;
         ontimes[key] = -1.0;
      }

      // check for tempo indication
      if (midifile[0][i][0] == 0xff &&
                 midifile[0][i][1] == 0x51) {
         setTempo(midifile, i, tempo);
      }
   }
}

//////////////////////////////
//
// setTempo -- set the current tempo
//
void setTempo(MidiFile& midifile, int index, double& tempo) {
   static int count = 0;
   count++;

   MidiEvent& mididata = midifile[0][index];

   int microseconds = 0;
   microseconds = microseconds | (mididata[3] << 16);
   microseconds = microseconds | (mididata[4] << 8);
   microseconds = microseconds | (mididata[5] << 0);

   double newtempo = 60.0 / microseconds * 1000000.0;
   if (count <= 1) {
      tempo = newtempo;
   } else if (tempo != newtempo) {
      cout << "; WARNING: change of tempo from " << tempo
           << " to " << newtempo << " ignored" << endl;
   }
}



void convertTextToMidiFile(istream& textfile, MidiFile& midifile) {
   vector<uchar> mididata;
   midifile.setTicksPerQuarterNote(tpq);
   midifile.absoluteTicks();
   midifile.allocateEvents(0, 2 * maxcount + 500);  // pre allocate space for
                                                    // max expected MIDI events

   // write the tempo to the midifile
   mididata.resize(6);
   mididata[0] = 0xff;      // meta message
   mididata[1] = 0x51;      // tempo change
   mididata[2] = 0x03;      // three bytes to follow
   int microseconds = (int)(60.0 / tempo * 1000000.0 + 0.5);
   mididata[3] = (microseconds >> 16) & 0xff;
   mididata[4] = (microseconds >> 8)  & 0xff;
   mididata[5] = (microseconds >> 0)  & 0xff;
   midifile.addEvent(0, 0, mididata);

   char buffer[1024] = {0};
   int line = 1;
   double start = 0.0;
   double dur = 0.0;
   int ontick = 0;
   int offtick = 0;
   int note = 0;
   int vel = 0;
   int eventtype = 0;

   while (!textfile.eof()) {
      textfile.getline(buffer, 1000, '\n');
      if (textfile.eof()) {
         break;
      }
      adjustbuffer(buffer);
      if (debugQ) {
         cout << "line " << line << ":\t" << buffer << endl;
      }
      readvalues(buffer, eventtype, start, dur, note, vel);
      if (eventtype != 1) {
         continue;
      }

      // have a good note, so store it in the MIDI file
      ontick  = (int)(start * tpq * 2.0 + 0.5);
      offtick = (int)((start + dur) * tpq * 2.0 + 0.5);
      if (offtick <= ontick) {
         offtick = ontick + 1;
      }
      if (debugQ) {
         cout << "Note ontick=" << ontick << "\tofftick=" << offtick
              << "\tnote=" << note << "\tvel=" << vel << endl;
      }

      mididata.resize(3);
      mididata[0] = 0x90 | channel;       // note on command
      mididata[1] = (uchar)(note & 0x7f);
      mididata[2] = (uchar)(vel & 0x7f);
      midifile.addEvent(0, ontick, mididata);
      mididata[0] = 0x80 | channel;       // note off command
      midifile.addEvent(0, offtick, mididata);

      line++;
   }
}



//////////////////////////////
//
// readvalues -- read parameter values from the input dataline.
//     returns 0 if no parameters were readable.
//

void readvalues(char* buffer, int& eventtype, double& start, double& dur,
   int& note, int& vel) {
   char *ptr = strtok(buffer, " \t\n");
   if (ptr == NULL) {
      eventtype = 0;
      return;
   }

   if (strcmp(ptr, "note") != 0) {
      eventtype = 0;
      return;
   } else {
      eventtype = 1;
   }

   // read the starttime
   ptr = strtok(NULL, " \t\n");
   if (ptr == NULL) {
      eventtype = 0;
      return;
   }
   start = atof(ptr);

   // read the duration
   ptr = strtok(NULL, " \t\n");
   if (ptr == NULL) {
      eventtype = 0;
      return;
   }
   dur = atof(ptr);

   // read the note number
   ptr = strtok(NULL, " \t\n");
   if (ptr == NULL) {
      eventtype = 0;
      return;
   }
   note = strtol(ptr, NULL, 10);
   if (note < 0 || note > 127) {
      eventtype = 0;
      return;
   }

   // read the starttime
   ptr = strtok(NULL, " \t\n");
   if (ptr == NULL) {
      eventtype = 0;
      return;
   }
   vel = strtol(ptr, NULL, 10);
   if (vel < 0 || vel > 127) {
      eventtype = 0;
      return;
   }

   eventtype = 1;

}



//////////////////////////////
//
// adjustbuffer -- remove comments and make lower characters
//

void adjustbuffer(char* buffer) {
   int i = 0;
   while (buffer[i] != '\0') {
      buffer[i] = tolower(buffer[i]);
      if (buffer[i] == ';') {
         buffer[i] = '\0';
         return;
      }
      i++;
   }
}
