//
// Created by Shihao Jing on 4/18/17.
//
#include <math.h>
#include <stdint.h>

#ifndef HANDMADE_H
#define HANDMADE_H

/*
 +  NOTE(casey):
 +
 +  HANDMADE_INTERNAL:
 +    0 - Build for public release
 +    1 - Build for developer only
 +
 +  HANDMADE_SLOW:
 +    0 - Not slow code allowed!
 +    1 - Slow code welcome.
 +*/

#if HANDMADE_SLOW

#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Pi32 3.14159265358979f

#define internal static
#define global_variable static
#define local_persist static

typedef float real32;
typedef double real64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct game_memory
{
  bool32 IsInitialized;

  uint64 PermanentStorageSize;
  void* PermanentStorage;
  uint64 TransientStorageSize;
  void* TransientStorage;
};

struct game_offscreen_buffer
{
  void* Memory;
  int Width;
  int Height;
  int Pitch;
};

struct game_sound_output_buffer
{
  int SamplesPerSecond;
  int SampleCount;
  int16 *Samples;
};

struct game_state
{
  int BlueOffset;
  int GreenOffset;
  int ToneHz;
};

struct game_button_state
{
  int HalfTransitionCount;
  bool32 EndedDown;
};

struct game_controller_input
{
  bool32 IsAnalog;

  real32 StartX;
  real32 StartY;

  real32 MinX;
  real32 MinY;

  real32 MaxX;
  real32 MaxY;

  real32 EndX;
  real32 EndY;

  union
  {
    game_button_state Buttons[6];
    struct
    {
      game_button_state Up;
      game_button_state Down;
      game_button_state Left;
      game_button_state Right;
      game_button_state LeftShoulder;
      game_button_state RightShoulder;
    };
  };
};

struct game_input
{
  game_controller_input Controllers[4];
};


internal void
GameUpdateAndRender(game_memory* Memory, game_input* Input,
                    game_offscreen_buffer* Buffer,
                    game_sound_output_buffer* SoundBuffer);

#endif //HANDMADE_H
