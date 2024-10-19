#define SDL_MAIN_USE_CALLBACKS 1  // use the callbacks instead of main()
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define MAX_WAVES 32

typedef struct WaveData
{
    SDL_AudioStream *stream;
    SDL_AudioSpec spec;
    char *desc;
    float *buffer;
    int buffer_len;
    Uint8 r, g, b;
    Uint64 start_ticks;
    Uint64 total_ticks;
} WaveData;


static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioDeviceID audio_devid = 0;
static char *failure_string = NULL;
static SDL_Joystick *joysticks[32];
static WaveData waves[MAX_WAVES];
static int buttons[MAX_WAVES];

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_Delay(1000);  // sleep for a second in case other services are chewing the CPU at startup.

    Uint32 winflags = SDL_WINDOW_FULLSCREEN;
    SDL_SetAppMetadata("soundboard", "1.0", "org.icculus.soundboard");

    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--windowed") == 0) {
            winflags &= ~SDL_WINDOW_FULLSCREEN;
        }
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
//SDL_Delay(1000);

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
//SDL_Delay(1000);

    if (!SDL_Init(SDL_INIT_JOYSTICK)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
//SDL_Delay(1000);

    if (!SDL_CreateWindowAndRenderer("Soundboard", 640, 480, winflags, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

//SDL_Delay(1000);

    SDL_HideCursor();

    SDL_SetRenderVSync(renderer, 1);

    audio_devid = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!audio_devid) {
        char *str = NULL;
        SDL_asprintf(&str, "Couldn't open audio device: %s", SDL_GetError());
        SDL_Log("%s", str);
        if (!failure_string) {
            failure_string = str;
        } else {
            SDL_free(str);
        }
    }

    int num_sticks = 0;
    SDL_JoystickID *sticks = SDL_GetJoysticks(&num_sticks);
    num_sticks = SDL_clamp(num_sticks, 0, SDL_arraysize(joysticks));

    bool opened_a_stick = false;
    for (int i = 0; i < num_sticks; i++) {
        joysticks[i] = SDL_OpenJoystick(sticks[i]);
        if (joysticks[i]) {
            opened_a_stick = true;
        }
    }
    SDL_free(sticks);

    if (!opened_a_stick) {
        char *str = SDL_strdup("Didn't open any joystick devices!");
        SDL_Log("%s", str);
        if (!failure_string) {
            failure_string = str;
        } else {
            SDL_free(str);
        }
    }

    bool loaded_a_wav = false;
    for (int i = 0; i < SDL_arraysize(waves); i++) {
        char *path = NULL;
        if (SDL_asprintf(&path, "%s%d.wav", SDL_GetBasePath(), i) > 0) {
            SDL_AudioSpec spec;
            Uint8 *buffer = NULL;
            Uint32 buffer_len = 0;
            if (SDL_LoadWAV(path, &spec, &buffer, &buffer_len)) {
                // an audio stream can convert on the fly, but we normalize all the .wav data to f32/2/44100 so we can visualize it without a lot of drama.
                bool okay = ((spec.format == SDL_AUDIO_F32) && (spec.channels == 2) && (spec.freq == 44100));
                if (okay) {
                    waves[i].buffer = (float *) buffer;
                    waves[i].buffer_len = buffer_len;
                    SDL_copyp(&waves[i].spec, &spec);
                    buffer = NULL;  // so we don't free it.
                } else {
                    waves[i].spec.format = SDL_AUDIO_F32;
                    waves[i].spec.channels = 2;
                    waves[i].spec.freq = 44100;
                    okay = SDL_ConvertAudioSamples(&spec, buffer, (int) buffer_len, &waves[i].spec, (Uint8 **) &waves[i].buffer, &waves[i].buffer_len);
                 }

                 if (okay) {
                    SDL_AudioStream *stream = SDL_CreateAudioStream(&waves[i].spec, NULL);
                    if (!SDL_BindAudioStream(audio_devid, stream)) {
                        SDL_DestroyAudioStream(stream);
                    } else {
                        waves[i].stream = stream;
                        waves[i].total_ticks = (Uint64) ((((waves[i].buffer_len / sizeof (float)) / 2) / 44100.0f) * 1000.0f);

                        char *txtpath = NULL;
                        if (SDL_asprintf(&txtpath, "%s%d.txt", SDL_GetBasePath(), i) > 0) {
                            waves[i].desc = (char *) SDL_LoadFile(txtpath, NULL);
                            if (waves[i].desc) {
                                char *ptr;
                                ptr = SDL_strchr(waves[i].desc, '\r');
                                if (ptr) {
                                    *ptr = '\0';
                                }
                                ptr = SDL_strchr(waves[i].desc, '\n');
                                if (ptr) {
                                    *ptr = '\0';
                                }
                            }
                            SDL_free(txtpath);
                        }

                        if (!waves[i].desc) {
                            SDL_asprintf(&waves[i].desc, "%d.wav", i);
                        }

                        loaded_a_wav = true;
                    }
                }
                SDL_free(buffer);
            }
            SDL_free(path);
        }
    }

    if (!loaded_a_wav) {
        char *str = SDL_strdup("Didn't load any wave files!");
        SDL_Log("%s", str);
        if (!failure_string) {
            failure_string = str;
        } else {
            SDL_free(str);
        }
    }

    #if 0
    SDL_srand(42);  // force a specific seed so we get the same "random" colors each time.
    for (int i = 0; i < SDL_arraysize(waves); i++) {
        waves[i].r = (Uint8) SDL_rand(256);
        waves[i].g = (Uint8) SDL_rand(256);
        waves[i].b = (Uint8) SDL_rand(256);
    }
    SDL_srand(0);  // reseed this from the clock in case we use this again.
    #else
    static const SDL_Color button_colors[] = {
        { 255, 255, 255, 255 },  // white
        { 255, 0, 0, 255 },      // red
        { 0, 255, 0, 255 },      // green
        { 0, 0, 255, 255 },      // blue
        { 255, 255, 0, 255 },    // yellow
        { 255, 255, 255, 255 },  // white
        { 255, 0, 0, 255 },      // red
        { 0, 255, 0, 255 },      // green
        { 0, 0, 255, 255 },      // blue
        { 255, 255, 0, 255 }     // yellow
    };

    for (int i = 0; i < SDL_arraysize(button_colors); i++) {
        waves[i].r = button_colors[i].r;
        waves[i].g = button_colors[i].g;
        waves[i].b = button_colors[i].b;
    }
    #endif

    return SDL_APP_CONTINUE;
}

static void HandleButton(const int button, const bool down)
{
    if (button >= SDL_arraysize(waves)) {
        return;  // skip this one, too many buttons.
    }

    buttons[button] += down ? 1 : -1;
    if (down && (buttons[button] == 1)) {
        WaveData *wav = &waves[button];
        if (wav->stream) {
            const int available = SDL_GetAudioStreamAvailable(wav->stream);
            SDL_ClearAudioStream(wav->stream);
            wav->start_ticks = 0;
            if (available == 0) {  // first hit starts the sound playing. second hit while still playing just stops it, in case you hit by accident.
                wav->start_ticks = SDL_GetTicks();
                SDL_PutAudioStreamData(wav->stream, wav->buffer, (int) wav->buffer_len);
                SDL_FlushAudioStream(wav->stream);
            }
        }
    }
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            } else if ((event->key.key >= SDLK_0) && (event->key.key <= SDLK_9)) {
                const int button = (int) (event->key.key - SDLK_0);
                const bool down = event->key.down;
                HandleButton(button, down);
            }
            break;

        case SDL_EVENT_JOYSTICK_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN: {
            const int button = (int) event->jbutton.button;
            const bool down = event->jbutton.down;
            HandleButton(button, down);
            break;
        }
    }
    return SDL_APP_CONTINUE;
}

static void RenderWaveform(const WaveData *wav, const int w, const int h, const Uint64 elapsed)
{
    SDL_SetRenderDrawColor(renderer, wav->r, wav->g, wav->b, 255);

    const int channels = 2;
    const float *buffer = wav->buffer;
    const int ticks_available = wav->total_ticks - elapsed;
    const float frames_per_ms = 44100.0f / 1000.0f;
    const int frame_offset = (int) (frames_per_ms * ((float) ticks_available));
    const int frame_size = (int) (sizeof (float) * channels);
    const int available = frame_offset * frame_size;
    const int buflen = SDL_min(available, 4096 * sizeof (float) * channels);

    if (buffer && buflen) {
        SDL_FPoint *points = SDL_stack_alloc(SDL_FPoint, w + 2);
        if (points) {
            buffer = (const float *) (((const Uint8 *) wav->buffer) + available);
            const int frames = (buflen / sizeof (float)) / channels;
            const int skip = frames / (w * 2);
            float prevx = 0.0f;
            int pointidx = 1;

            points[0].x = 0.0f;
            points[0].y = h * 0.5f;

            for (int i = 0; i < (w + 1); i++) {
                const float *bptr = &buffer[(i * skip) * channels];
                const float val = (bptr[0] + bptr[1]) / 2.0f;
                const float x = prevx + 2;
                const float y = (h * 0.5f) - (h * (val * 0.5f));
                SDL_assert(pointidx < (w + 2));
                points[pointidx].x = x;
                points[pointidx].y = y;
                pointidx++;
                prevx = x;
            }

            SDL_RenderLines(renderer, points, pointidx);
            SDL_stack_free(points);
        }
    }
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    int winw, winh;
    SDL_GetRenderOutputSize(renderer, &winw, &winh);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    const float w = ((float) winw) / SDL_arraysize(buttons);
    const float h = 30.0f;
    float x = 0.0f;
    float y = 0.0f;
    for (int i = 0; i < SDL_arraysize(buttons); i++) {
        if (buttons[i] > 0) {
            const WaveData *wav = &waves[i];
            const SDL_FRect rect = { x, y, w, h };
            SDL_SetRenderDrawColor(renderer, wav->r, wav->g, wav->b, 255);
            SDL_RenderFillRect(renderer, &rect);
        }
        x += w;
    }

    x = 50;
    y = 50;
    for (int i = 0; i < SDL_arraysize(waves); i++) {
        WaveData *wav = &waves[i];
        if (wav->desc) {
            SDL_SetRenderDrawColor(renderer, wav->r, wav->g, wav->b, 255);
            SDL_RenderDebugText(renderer, x, y, wav->desc);
            y += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2;
        }
    }

    const Uint64 now = SDL_GetTicks();
    for (int i = 0; i < SDL_arraysize(waves); i++) {
        WaveData *wav = &waves[i];
        if (!wav->start_ticks) {  // not playing.
            continue;
        } else {
            const Uint64 elapsed = now - wav->start_ticks;
            if (elapsed >= wav->total_ticks) {
                wav->start_ticks = 0;
            } else {
                RenderWaveform(wav, winw, winh, elapsed);
            }
        }
    }

    if (failure_string) {
        const float x = (winw - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * SDL_strlen(failure_string))) / 2;
        const float y = (winh - (SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE)) / 2;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, x, y, failure_string);
    }

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    SDL_CloseAudioDevice(audio_devid);

    for (int i = 0; i < SDL_arraysize(joysticks); i++) {
        SDL_CloseJoystick(joysticks[i]);
    }

    for (int i = 0; i < SDL_arraysize(waves); i++) {
        SDL_DestroyAudioStream(waves[i].stream);
        SDL_free(waves[i].desc);
        SDL_free(waves[i].buffer);
    }

    SDL_free(failure_string);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

