/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Shihao Jing $
   ======================================================================== */

/*
 * TODO: THIS NOT A FINAL PLATFORM LAYER!!!
 * - Saved game locations
 * - Getting a handle to our own executable file
 * - Asset loading path
 * - Threading (launch a thread)
 * - Raw Input (support for multiple keyboards
 * - Sleep/timeBeginPeriod
 * - ClipCursor() (for multimonitor support)
 * - Fullscreen support
 * - Control cursor visibility
 * - QueryCancelAutoplay
 * - Hardware acceleration
 * - GetKeyboardLayout
 *
 * Just a partial list of stuff!
 */

#include <SDL.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <math.h>
#include <x86intrin.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265357f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"
#include "sdl_handmade.h"


global_variable sdl_offscreen_buffer GlobalBackbuffer;

#define MAX_CONTROLLERS 4
SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];


sdl_audio_ring_buffer AudioRingBuffer;

void SDLFillSoundBuffer(sdl_sound_output *SoundOutput, int ByteToLock, int BytesToWrite,
                        game_sound_output_buffer *SoundBuffer)
{
  int16_t *Samples = SoundBuffer->Samples;
  // TODO: More strenuous test!
  void *Region1 = (uint8*)AudioRingBuffer.Data + ByteToLock;
  int Region1Size = BytesToWrite;
  if (Region1Size + ByteToLock > SoundOutput->SecondaryBufferSize)
    Region1Size = SoundOutput->SecondaryBufferSize - ByteToLock;

  void *Region2 = AudioRingBuffer.Data;
  int Region2Size = BytesToWrite - Region1Size;

  int Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
  int16 *SampleOut = (int16 *)Region1;
  for(int SampleIndex = 0; SampleIndex < Region1SampleCount;
      ++SampleIndex)
  {
    *SampleOut++ = *Samples++;
    *SampleOut++ = *Samples++;

    ++SoundOutput->RunningSampleIndex;
  }

  int Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
  SampleOut = (int16 *)Region2;
  for(int SampleIndex = 0; SampleIndex < Region2SampleCount;
      ++SampleIndex)
  {
    *SampleOut++ = *Samples++;
    *SampleOut++ = *Samples++;

    ++SoundOutput->RunningSampleIndex;
  }
}

internal void
SDLAudioCallback(void *UserData, Uint8 *AudioData, int Length)
{
  sdl_audio_ring_buffer *RingBuffer = (sdl_audio_ring_buffer *)UserData;

  int Region1Size = Length;
  int Region2Size = 0;
  if (RingBuffer->PlayCursor + Length > RingBuffer->Size)
  {
    Region1Size = RingBuffer->Size - RingBuffer->PlayCursor;
    Region2Size = Length - Region1Size;
  }
  memcpy(AudioData, (uint8*)(RingBuffer->Data) + RingBuffer->PlayCursor, Region1Size);
  memcpy(&AudioData[Region1Size], RingBuffer->Data, Region2Size);
  RingBuffer->PlayCursor = (RingBuffer->PlayCursor + Length) % RingBuffer->Size;
  //RingBuffer->WriteCursor = (RingBuffer->PlayCursor + 2048) % RingBuffer->Size;
}

internal void
SDLInitAudio(int32 SamplesPerSecond, int32 BufferSize)
{
  SDL_AudioSpec AudioSettings = {0};

  AudioSettings.freq = SamplesPerSecond;
  AudioSettings.format = AUDIO_S16LSB;
  AudioSettings.channels = 2;
  AudioSettings.samples = 1024;
  AudioSettings.callback = &SDLAudioCallback;
  AudioSettings.userdata = &AudioRingBuffer;

  AudioRingBuffer.Size = BufferSize;
  AudioRingBuffer.Data = calloc(BufferSize, 1);
  AudioRingBuffer.PlayCursor = AudioRingBuffer.WriteCursor = 0;

  SDL_OpenAudio(&AudioSettings, 0);

  printf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n",
         AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

  if (AudioSettings.format != AUDIO_S16LSB)
  {
    printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
    SDL_CloseAudio();
  }

}

sdl_window_dimension
SDLGetWindowDimension(SDL_Window *Window)
{
  sdl_window_dimension Result;

  SDL_GetWindowSize(Window, &Result.Width, &Result.Height);

  return(Result);
}

internal void
SDLResizeTexture(sdl_offscreen_buffer *Buffer, SDL_Renderer *Renderer, int Width, int Height)
{
  int BytesPerPixel = 4;
  if (Buffer->Memory)
  {
    munmap(Buffer->Memory,
           Buffer->Width * Buffer->Height * BytesPerPixel);
  }
  if (Buffer->Texture)
  {
    SDL_DestroyTexture(Buffer->Texture);
  }
  Buffer->Texture = SDL_CreateTexture(Renderer,
                                      SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      Width,
                                      Height);
  Buffer->Width = Width;
  Buffer->Height = Height;
  Buffer->Pitch = Width * BytesPerPixel;
  Buffer->Memory = mmap(0,
                        Width * Height * BytesPerPixel,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
}

internal void
SDLUpdateWindow(SDL_Window *Window, SDL_Renderer *Renderer, sdl_offscreen_buffer *Buffer)
{
  SDL_UpdateTexture(Buffer->Texture,
                    0,
                    Buffer->Memory,
                    Buffer->Pitch);

  SDL_RenderCopy(Renderer,
                 Buffer->Texture,
                 0,
                 0);

  SDL_RenderPresent(Renderer);
}


bool HandleEvent(SDL_Event *Event)
{
  bool ShouldQuit = false;

  switch(Event->type)
  {
  case SDL_QUIT:
  {
    printf("SDL_QUIT\n");
    ShouldQuit = true;
  } break;

  case SDL_KEYDOWN:
  case SDL_KEYUP:
  {
    SDL_Keycode KeyCode = Event->key.keysym.sym;
    bool IsDown = (Event->key.state == SDL_PRESSED);
    bool WasDown = false;
    if (Event->key.state == SDL_RELEASED)
    {
      WasDown = true;
    }
    else if (Event->key.repeat != 0)
    {
      WasDown = true;
    }

    // NOTE: In the windows version, we used "if (IsDown != WasDown)"
    // to detect key repeats. SDL has the 'repeat' value, though,
    // which we'll use.
    if (Event->key.repeat == 0)
    {
      if(KeyCode == SDLK_w)
      {
      }
      else if(KeyCode == SDLK_a)
      {
      }
      else if(KeyCode == SDLK_s)
      {
      }
      else if(KeyCode == SDLK_d)
      {
      }
      else if(KeyCode == SDLK_q)
      {
      }
      else if(KeyCode == SDLK_e)
      {
      }
      else if(KeyCode == SDLK_UP)
      {
      }
      else if(KeyCode == SDLK_LEFT)
      {
      }
      else if(KeyCode == SDLK_DOWN)
      {
      }
      else if(KeyCode == SDLK_RIGHT)
      {
      }
      else if(KeyCode == SDLK_ESCAPE)
      {
        printf("ESCAPE: ");
        if(IsDown)
        {
          printf("IsDown ");
        }
        if(WasDown)
        {
          printf("WasDown");
        }
        printf("\n");
      }
      else if(KeyCode == SDLK_SPACE)
      {
      }
    }

    bool AltKeyWasDown = (Event->key.keysym.mod & KMOD_ALT);
    if (KeyCode == SDLK_F4 && AltKeyWasDown)
    {
      ShouldQuit = true;
    }

  } break;

  case SDL_WINDOWEVENT:
  {
    switch(Event->window.event)
    {
    case SDL_WINDOWEVENT_SIZE_CHANGED:
    {
      SDL_Window *Window = SDL_GetWindowFromID(Event->window.windowID);
      SDL_Renderer *Renderer = SDL_GetRenderer(Window);
      printf("SDL_WINDOWEVENT_SIZE_CHANGED (%d, %d)\n", Event->window.data1, Event->window.data2);
    } break;

    case SDL_WINDOWEVENT_FOCUS_GAINED:
    {
      printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
    } break;

    case SDL_WINDOWEVENT_EXPOSED:
    {
      SDL_Window *Window = SDL_GetWindowFromID(Event->window.windowID);
      SDL_Renderer *Renderer = SDL_GetRenderer(Window);
      SDLUpdateWindow(Window, Renderer, &GlobalBackbuffer);
    } break;
    }
  } break;
  }

  return(ShouldQuit);
}

internal void
SDLOpenGameControllers()
{
  int MaxJoysticks = SDL_NumJoysticks();
  int ControllerIndex = 0;
  for(int JoystickIndex=0; JoystickIndex < MaxJoysticks; ++JoystickIndex)
  {
    if (!SDL_IsGameController(JoystickIndex))
    {
      continue;
    }
    if (ControllerIndex >= MAX_CONTROLLERS)
    {
      break;
    }
    ControllerHandles[ControllerIndex] = SDL_GameControllerOpen(JoystickIndex);
    SDL_Joystick *JoystickHandle = SDL_GameControllerGetJoystick(ControllerHandles[ControllerIndex]);
    RumbleHandles[ControllerIndex] = SDL_HapticOpenFromJoystick(JoystickHandle);
    if (SDL_HapticRumbleInit(RumbleHandles[ControllerIndex]) != 0)
    {
      SDL_HapticClose(RumbleHandles[ControllerIndex]);
      RumbleHandles[ControllerIndex] = 0;
    }

    ControllerIndex++;
  }
}

internal void
SDLCloseGameControllers()
{
  for(int ControllerIndex = 0; ControllerIndex < MAX_CONTROLLERS; ++ControllerIndex)
  {
    if (ControllerHandles[ControllerIndex])
    {
      if (RumbleHandles[ControllerIndex])
        SDL_HapticClose(RumbleHandles[ControllerIndex]);
      SDL_GameControllerClose(ControllerHandles[ControllerIndex]);
    }
  }
}


internal void
SDLProcessGamecontrollerButton(game_button_state  *OldState, game_button_state *NewState,
                               SDL_GameController *ControllerHandle, SDL_GameControllerButton Button)
{
  NewState->EndedDown = SDL_GameControllerGetButton(ControllerHandle, Button);
  NewState->HalfTransitionCount += ((NewState->EndedDown == OldState->EndedDown) ? 0 : 1);
}

int main(int argc, char *argv[])
{
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO);
  uint64 PerfCountFrequency = SDL_GetPerformanceFrequency();
  // Initialise our Game Controllers:
  SDLOpenGameControllers();
  // Create our window.
  SDL_Window *Window = SDL_CreateWindow("Handmade Hero",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        640,
                                        480,
                                        SDL_WINDOW_RESIZABLE);
  if(Window)
  {
    // Create a "Renderer" for our window.
    SDL_Renderer *Renderer = SDL_CreateRenderer(Window,
                                                -1,
                                                SDL_RENDERER_PRESENTVSYNC);
    if (Renderer)
    {
      bool Running = true;
      sdl_window_dimension Dimension = SDLGetWindowDimension(Window);
      SDLResizeTexture(&GlobalBackbuffer, Renderer, Dimension.Width, Dimension.Height);
      int XOffset = 0;
      int YOffset = 0;

      game_input Input[2] = {};
      game_input *NewInput = &Input[0];
      game_input *OldInput = &Input[1];


      // NOTE: Sound test
      sdl_sound_output SoundOutput = {};
      SoundOutput.SamplesPerSecond = 48000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(int16) * 2;
      SoundOutput.SecondaryBufferSize = SoundOutput. SamplesPerSecond * SoundOutput.BytesPerSample;
      SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
      // Open our audio device:
      SDLInitAudio(48000, SoundOutput.SecondaryBufferSize);
      int16 *Samples = (int16 *)calloc(SoundOutput.SamplesPerSecond, SoundOutput.BytesPerSample);

      SDL_PauseAudio(0);

      uint64 LastCounter = SDL_GetPerformanceCounter();
      uint64 LastCycleCount = _rdtsc();
      while(Running)
      {
        SDL_Event Event;
        while(SDL_PollEvent(&Event))
        {
          if (HandleEvent(&Event))
          {
            Running = false;
          }
        }

        // Poll our controllers for input.
        for (int ControllerIndex = 0;
             ControllerIndex < MAX_CONTROLLERS;
             ++ControllerIndex)
        {
          if(ControllerHandles[ControllerIndex] != 0 && SDL_GameControllerGetAttached(ControllerHandles[ControllerIndex]))
          {
            game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
            game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

            NewController->IsAnalog = true;
            // NOTE: We have a controller with index ControllerIndex.
            bool Up = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_UP);
            bool Down = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bool Left = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool Right = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            bool Start = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_START);
            bool Back = SDL_GameControllerGetButton(ControllerHandles[ControllerIndex], SDL_CONTROLLER_BUTTON_BACK);

            SDLProcessGamecontrollerButton(&(OldController->LeftShoulder),
                                           &(NewController->LeftShoulder),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_LEFTSHOULDER);

            SDLProcessGamecontrollerButton(&(OldController->RightShoulder),
                                           &(NewController->RightShoulder),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

            SDLProcessGamecontrollerButton(&(OldController->Down),
                                           &(NewController->Down),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_A);

            SDLProcessGamecontrollerButton(&(OldController->Right),
                                           &(NewController->Right),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_B);

            SDLProcessGamecontrollerButton(&(OldController->Left),
                                           &(NewController->Left),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_X);

            SDLProcessGamecontrollerButton(&(OldController->Right),
                                           &(NewController->Right),
                                           ControllerHandles[ControllerIndex],
                                           SDL_CONTROLLER_BUTTON_Y);

            int16 StickX = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTX);
            int16 StickY = SDL_GameControllerGetAxis(ControllerHandles[ControllerIndex], SDL_CONTROLLER_AXIS_LEFTY);

            if (StickX < 0)
            {
              NewController->EndX = StickX / -32768.0f;
            }
            else
            {
              NewController->EndX = StickX / -32767.0f;
            }

            NewController->MinX = NewController->MaxX = NewController->EndX;

            if (StickY < 0)
            {
              NewController->EndY = StickY / -32768.0f;
            }
            else
            {
              NewController->EndY = StickY / -32767.0f;
            }
            NewController->MinY = NewController->MaxY = NewController->EndY;
          }
          else
          {
            // TODO: This controller is not plugged in.
          }
        }


        // Sound output test
        SDL_LockAudio();
        int ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
        int TargetCursor = ((AudioRingBuffer.PlayCursor +
            (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) %
            SoundOutput.SecondaryBufferSize);
        int BytesToWrite = 0;
        if(ByteToLock > TargetCursor)
        {
          BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
          BytesToWrite += TargetCursor;
        }
        else
        {
          BytesToWrite = TargetCursor - ByteToLock;
        }
        SDL_UnlockAudio();

        game_sound_output_buffer SoundBuffer = {};
        SoundBuffer.Samples = Samples;
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;

        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackbuffer.Memory;
        Buffer.Width = GlobalBackbuffer.Width;
        Buffer.Height = GlobalBackbuffer.Height;
        Buffer.Pitch = GlobalBackbuffer.Pitch;

        GameUpdateAndRender(NewInput, &Buffer, &SoundBuffer);

        game_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        SDLFillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
        SDLUpdateWindow(Window, Renderer, &GlobalBackbuffer);

        uint64 EndCycleCount = _rdtsc();
        uint64 EndCounter = SDL_GetPerformanceCounter();
        uint64 CounterElapsed = EndCounter - LastCounter;
        uint64 CyclesElapsed = EndCycleCount - LastCycleCount;

        real64 MSPerFrame = (((1000.0f * (real64)CounterElapsed) / (real64)PerfCountFrequency));
        real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
        real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

#if 0
        printf("%.02fms/f, %.02f/s, %.02fmc/f\n", MSPerFrame, FPS, MCPF);
#endif

        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;

      }
    }
    else
    {
      // TODO(casey): Logging
    }
  }
  else
  {
    // TODO(casey): Logging
  }

  SDLCloseGameControllers();
  SDL_Quit();
  return(0);
}
