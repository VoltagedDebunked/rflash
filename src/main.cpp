#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080
#define MAP_WIDTH 50
#define MAP_HEIGHT 50
#define TEXTURE_SIZE 64
#define NUM_TEXTURES 4
#define MINIMAP_SCALE 8

typedef struct
{
  double posX;
  double posY;
  double dirX;
  double dirY;
  double planeX;
  double planeY;
  double moveSpeed;
  double rotSpeed;
  double stamina;
  bool sprinting;
} Player;

typedef struct
{
  int x;
  int y;
  int parent_x;
  int parent_y;
  double g_cost;
  double h_cost;
  double f_cost;
} Node;

typedef struct
{
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *screenTexture;
  uint32_t *screenBuffer;
  uint32_t textures[NUM_TEXTURES][TEXTURE_SIZE * TEXTURE_SIZE];
  Player player;
  int map[MAP_WIDTH][MAP_HEIGHT];
  bool running;
  Uint32 lastFrame;
  double deltaTime;
  bool showMinimap;
  Mix_Chunk *footstepSound;
  Mix_Chunk *sprintSound;
  Mix_Music *backgroundMusic;
} GameState;

// Function prototypes
bool initGame (GameState *game);
void cleanupGame (GameState *game);
void handleInput (GameState *game);
void renderFrame (GameState *game);
void generateMaze (GameState *game);
void generateTextures (GameState *game);
void renderMinimap (GameState *game);
double getDeltaTime (GameState *game);
void updatePlayerStamina (GameState *game);

// Initialize game
bool
initGame (GameState *game)
{
  if (SDL_Init (SDL_INIT_VIDEO) < 0)
    return false;

  game->window = SDL_CreateWindow ("Enhanced 3D Maze", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                   WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
  if (!game->window)
    return false;

  game->renderer = SDL_CreateRenderer (
      game->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!game->renderer)
    return false;

  game->screenTexture = SDL_CreateTexture (
      game->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      WINDOW_WIDTH, WINDOW_HEIGHT);
  if (!game->screenTexture)
    return false;

  game->screenBuffer
      = (uint32_t *)malloc (WINDOW_WIDTH * WINDOW_HEIGHT * sizeof (uint32_t));
  if (!game->screenBuffer)
    return false;

  if (Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
      printf ("SDL_mixer could not initialize! SDL_mixer Error: %s\n",
              Mix_GetError ());
      return false;
    }

  // Load sound effects
  game->footstepSound = Mix_LoadWAV ("assets/footstep.wav");
  game->sprintSound = Mix_LoadWAV ("assets/sprint.wav");
  if (!game->footstepSound || !game->sprintSound)
    {
      printf ("Failed to load sound effect! SDL_mixer Error: %s\n",
              Mix_GetError ());
      return false;
    }

  // Load background music
  game->backgroundMusic = Mix_LoadMUS ("assets/background.mp3");
  if (!game->backgroundMusic)
    {
      printf ("Failed to load background music! SDL_mixer Error: %s\n",
              Mix_GetError ());
      return false;
    }

  // Start playing background music
  Mix_PlayMusic (game->backgroundMusic, -1); // -1 means loop indefinitely

  // Initialize player
  game->player.posX = 1.5;
  game->player.posY = 1.5;
  game->player.dirX = 1.0;
  game->player.dirY = 0.0;
  game->player.planeX = 0.0;
  game->player.planeY = 0.66;
  game->player.moveSpeed = 0.05;
  game->player.rotSpeed = 0.03;
  game->player.stamina = 100.0;
  game->player.sprinting = false;

  game->lastFrame = SDL_GetTicks ();
  game->showMinimap = true;

  generateTextures (game);
  generateMaze (game);
  game->running = true;

  return true;
}

// Generate procedural textures
void
generateTextures (GameState *game)
{
  for (int t = 0; t < NUM_TEXTURES; t++)
    {
      for (int x = 0; x < TEXTURE_SIZE; x++)
        {
          for (int y = 0; y < TEXTURE_SIZE; y++)
            {
              uint32_t color = 0xFF000000;
              switch (t)
                {
                case 0: // Brick pattern
                  if ((x % 16 < 2) || (y % 16 < 2))
                    color |= 0x444444;
                  else
                    color |= 0x993333;
                  break;
                case 1: // Stone pattern
                  color |= ((x ^ y) % 8 == 0) ? 0x666666 : 0x888888;
                  break;
                case 2: // Wood pattern
                  color |= ((x / 4 + y / 4) % 2) ? 0x8B4513 : 0x654321;
                  break;
                case 3: // Metal pattern
                  {
                    int val = (x * y) % 64 + 128;
                    color |= (val << 16) | (val << 8) | val;
                  }
                  break;
                }
              game->textures[t][y * TEXTURE_SIZE + x] = color;
            }
        }
    }
}

// Generate maze using recursive division
void
generateMaze (GameState *game)
{
  // Initialize empty maze
  memset (game->map, 0, sizeof (game->map));

  // Create outer walls
  for (int x = 0; x < MAP_WIDTH; x++)
    {
      game->map[x][0] = 1;
      game->map[x][MAP_HEIGHT - 1] = 1;
    }
  for (int y = 0; y < MAP_HEIGHT; y++)
    {
      game->map[0][y] = 1;
      game->map[MAP_WIDTH - 1][y] = 1;
    }

  // Recursive division function
  srand (time (NULL));

  // Add random walls and pillars
  for (int i = 2; i < MAP_WIDTH - 2; i += 2)
    {
      for (int j = 2; j < MAP_HEIGHT - 2; j += 2)
        {
          if (rand () % 3 == 0)
            {
              game->map[i][j] = rand () % NUM_TEXTURES + 1;

              // Sometimes extend walls
              if (rand () % 2 == 0)
                {
                  int dir = rand () % 4;
                  int len = rand () % 3 + 1;
                  int dx[] = { 1, 0, -1, 0 };
                  int dy[] = { 0, 1, 0, -1 };

                  for (int k = 1; k <= len; k++)
                    {
                      int newX = i + dx[dir] * k;
                      int newY = j + dy[dir] * k;
                      if (newX > 0 && newX < MAP_WIDTH - 1 && newY > 0
                          && newY < MAP_HEIGHT - 1)
                        {
                          game->map[newX][newY] = rand () % NUM_TEXTURES + 1;
                        }
                    }
                }
            }
        }
    }

  // Ensure player starting area is clear
  game->map[1][1] = 0;
  game->map[1][2] = 0;
  game->map[2][1] = 0;
}

// Handle player input with swapped A and D controls
void
handleInput (GameState *game)
{
  SDL_Event event;

  const uint8_t *keys = SDL_GetKeyboardState (NULL);

  if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S])
    {
      if (game->player.sprinting)
        {
          Mix_PlayChannel (-1, game->sprintSound, 0);
        }
      else
        {
          Mix_PlayChannel (-1, game->footstepSound, 0);
        }
    }

  while (SDL_PollEvent (&event))
    {
      if (event.type == SDL_QUIT)
        {
          game->running = false;
        }
      else if (event.type == SDL_KEYDOWN)
        {
          if (event.key.keysym.scancode == SDL_SCANCODE_M)
            {
              game->showMinimap = !game->showMinimap;
            }
        }
    }

  // Update sprint state and speed
  game->player.sprinting
      = keys[SDL_SCANCODE_LSHIFT] && game->player.stamina > 0;
  double moveSpeed = game->player.moveSpeed
                     * (game->player.sprinting ? 2.0 : 1.0) * game->deltaTime
                     * 60.0;
  double rotSpeed = game->player.rotSpeed * game->deltaTime * 60.0;

  // Move forward
  if (keys[SDL_SCANCODE_W])
    {
      double newX = game->player.posX + game->player.dirX * moveSpeed;
      double newY = game->player.posY + game->player.dirY * moveSpeed;

      if (game->map[(int)newX][(int)game->player.posY] == 0)
        game->player.posX = newX;
      if (game->map[(int)game->player.posX][(int)newY] == 0)
        game->player.posY = newY;
    }

  // Move backward
  if (keys[SDL_SCANCODE_S])
    {
      double newX = game->player.posX - game->player.dirX * moveSpeed;
      double newY = game->player.posY - game->player.dirY * moveSpeed;

      if (game->map[(int)newX][(int)game->player.posY] == 0)
        game->player.posX = newX;
      if (game->map[(int)game->player.posX][(int)newY] == 0)
        game->player.posY = newY;
    }

  // Rotate right (was left)
  if (keys[SDL_SCANCODE_D])
    {
      double oldDirX = game->player.dirX;
      game->player.dirX = game->player.dirX * cos (rotSpeed)
                          - game->player.dirY * sin (rotSpeed);
      game->player.dirY
          = oldDirX * sin (rotSpeed) + game->player.dirY * cos (rotSpeed);
      double oldPlaneX = game->player.planeX;
      game->player.planeX = game->player.planeX * cos (rotSpeed)
                            - game->player.planeY * sin (rotSpeed);
      game->player.planeY
          = oldPlaneX * sin (rotSpeed) + game->player.planeY * cos (rotSpeed);
    }

  // Rotate left (was right)
  if (keys[SDL_SCANCODE_A])
    {
      double oldDirX = game->player.dirX;
      game->player.dirX = game->player.dirX * cos (-rotSpeed)
                          - game->player.dirY * sin (-rotSpeed);
      game->player.dirY
          = oldDirX * sin (-rotSpeed) + game->player.dirY * cos (-rotSpeed);
      double oldPlaneX = game->player.planeX;
      game->player.planeX = game->player.planeX * cos (-rotSpeed)
                            - game->player.planeY * sin (-rotSpeed);
      game->player.planeY = oldPlaneX * sin (-rotSpeed)
                            + game->player.planeY * cos (-rotSpeed);
    }
}

// Update player stamina
void
updatePlayerStamina (GameState *game)
{
  if (game->player.sprinting)
    {
      game->player.stamina -= 30.0 * game->deltaTime;
      if (game->player.stamina < 0)
        game->player.stamina = 0;
    }
  else
    {
      game->player.stamina += 10.0 * game->deltaTime;
      if (game->player.stamina > 100)
        game->player.stamina = 100;
    }
}

// Calculate delta time
double
getDeltaTime (GameState *game)
{
  Uint32 currentFrame = SDL_GetTicks ();
  double deltaTime = (currentFrame - game->lastFrame) / 1000.0;
  game->lastFrame = currentFrame;
  return deltaTime;
}

// Render minimap
void
renderMinimap (GameState *game)
{
  int minimap_size = 150;
  int border = 10;

  // Draw background
  SDL_Rect mapRect = { border, border, minimap_size, minimap_size };
  for (int x = 0; x < MAP_WIDTH; x++)
    {
      for (int y = 0; y < MAP_HEIGHT; y++)
        {
          int screenX = border + (x * minimap_size / MAP_WIDTH);
          int screenY = border + (y * minimap_size / MAP_HEIGHT);
          int width = minimap_size / MAP_WIDTH;
          int height = minimap_size / MAP_HEIGHT;

          uint32_t color = game->map[x][y] ? 0xFFFFFFFF : 0xFF333333;

          for (int px = 0; px < width; px++)
            {
              for (int py = 0; py < height; py++)
                {
                  int idx = (screenY + py) * WINDOW_WIDTH + (screenX + px);
                  if (idx >= 0 && idx < WINDOW_WIDTH * WINDOW_HEIGHT)
                    {
                      game->screenBuffer[idx] = color;
                    }
                }
            }
        }
    }

  // Draw player
  int playerScreenX
      = border + (int)(game->player.posX * minimap_size / MAP_WIDTH);
  int playerScreenY
      = border + (int)(game->player.posY * minimap_size / MAP_HEIGHT);

  for (int px = -2; px <= 2; px++)
    {
      for (int py = -2; py <= 2; py++)
        {
          int idx = (playerScreenY + py) * WINDOW_WIDTH + (playerScreenX + px);
          if (idx >= 0 && idx < WINDOW_WIDTH * WINDOW_HEIGHT)
            {
              game->screenBuffer[idx] = 0xFFFF0000;
            }
        }
    }
}

// Render a single frame with textured walls
void
renderFrame (GameState *game)
{
  memset (game->screenBuffer, 0,
          WINDOW_WIDTH * WINDOW_HEIGHT * sizeof (uint32_t));

  for (int x = 0; x < WINDOW_WIDTH; x++)
    {
      double cameraX = 2 * x / (double)WINDOW_WIDTH - 1;
      double rayDirX = game->player.dirX + game->player.planeX * cameraX;
      double rayDirY = game->player.dirY + game->player.planeY * cameraX;

      int mapX = (int)game->player.posX;
      int mapY = (int)game->player.posY;

      double sideDistX, sideDistY;
      double deltaDistX = fabs (1 / rayDirX);
      double deltaDistY = fabs (1 / rayDirY);
      double perpWallDist;

      int stepX, stepY;
      int hit = 0;
      int side;
      int texNum;

      if (rayDirX < 0)
        {
          stepX = -1;
          sideDistX = (game->player.posX - mapX) * deltaDistX;
        }
      else
        {
          stepX = 1;
          sideDistX = (mapX + 1.0 - game->player.posX) * deltaDistX;
        }
      if (rayDirY < 0)
        {
          stepY = -1;
          sideDistY = (game->player.posY - mapY) * deltaDistY;
        }
      else
        {
          stepY = 1;
          sideDistY = (mapY + 1.0 - game->player.posY) * deltaDistY;
        }

      while (hit == 0)
        {
          if (sideDistX < sideDistY)
            {
              sideDistX += deltaDistX;
              mapX += stepX;
              side = 0;
            }
          else
            {
              sideDistY += deltaDistY;
              mapY += stepY;
              side = 1;
            }

          if (game->map[mapX][mapY] > 0)
            {
              hit = 1;
              texNum = game->map[mapX][mapY] - 1;
            }
        }

      // Calculate wall distance and height
      if (side == 0)
        perpWallDist = (mapX - game->player.posX + (1 - stepX) / 2) / rayDirX;
      else
        perpWallDist = (mapY - game->player.posY + (1 - stepY) / 2) / rayDirY;

      int lineHeight = (int)(WINDOW_HEIGHT / perpWallDist);

      int drawStart = -lineHeight / 2 + WINDOW_HEIGHT / 2;
      if (drawStart < 0)
        drawStart = 0;
      int drawEnd = lineHeight / 2 + WINDOW_HEIGHT / 2;
      if (drawEnd >= WINDOW_HEIGHT)
        drawEnd = WINDOW_HEIGHT - 1;

      // Calculate texture coordinates
      double wallX;
      if (side == 0)
        wallX = game->player.posY + perpWallDist * rayDirY;
      else
        wallX = game->player.posX + perpWallDist * rayDirX;
      wallX -= floor (wallX);

      int texX = (int)(wallX * TEXTURE_SIZE);
      if (side == 0 && rayDirX > 0)
        texX = TEXTURE_SIZE - texX - 1;
      if (side == 1 && rayDirY < 0)
        texX = TEXTURE_SIZE - texX - 1;

      // Draw the textured wall
      double step = 1.0 * TEXTURE_SIZE / lineHeight;
      double texPos = (drawStart - WINDOW_HEIGHT / 2 + lineHeight / 2) * step;

      for (int y = drawStart; y < drawEnd; y++)
        {
          int texY = (int)texPos & (TEXTURE_SIZE - 1);
          texPos += step;

          uint32_t color = game->textures[texNum][TEXTURE_SIZE * texY + texX];

          // Apply distance shading
          double shade = 1.0 / (perpWallDist * 0.5 + 1.0);
          uint8_t r = ((color >> 16) & 0xFF) * shade;
          uint8_t g = ((color >> 8) & 0xFF) * shade;
          uint8_t b = (color & 0xFF) * shade;

          color = (0xFF << 24) | (r << 16) | (g << 8) | b;

          if (side == 1)
            { // Darken y-sides
              r = r * 0.7;
              g = g * 0.7;
              b = b * 0.7;
              color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            }

          game->screenBuffer[y * WINDOW_WIDTH + x] = color;
        }

      // Floor and ceiling casting
      for (int y = drawEnd + 1; y < WINDOW_HEIGHT; y++)
        {
          double currentDist = WINDOW_HEIGHT / (2.0 * y - WINDOW_HEIGHT);

          double weight = currentDist / perpWallDist;

          double currentFloorX
              = weight * wallX + (1.0 - weight) * game->player.posX;
          double currentFloorY
              = weight * wallX + (1.0 - weight) * game->player.posY;

          int floorTexX
              = (int)(currentFloorX * TEXTURE_SIZE) & (TEXTURE_SIZE - 1);
          int floorTexY
              = (int)(currentFloorY * TEXTURE_SIZE) & (TEXTURE_SIZE - 1);

          // Draw floor
          uint32_t floorColor
              = game->textures[3][TEXTURE_SIZE * floorTexY + floorTexX];
          double floorShade = 1.0 / (currentDist * 0.5 + 1.0);
          uint8_t fr = ((floorColor >> 16) & 0xFF) * floorShade * 0.5;
          uint8_t fg = ((floorColor >> 8) & 0xFF) * floorShade * 0.5;
          uint8_t fb = (floorColor & 0xFF) * floorShade * 0.5;
          game->screenBuffer[y * WINDOW_WIDTH + x]
              = (0xFF << 24) | (fr << 16) | (fg << 8) | fb;

          // Draw ceiling
          uint32_t ceilColor
              = game->textures[2][TEXTURE_SIZE * floorTexY + floorTexX];
          double ceilShade = 1.0 / (currentDist * 0.5 + 1.0);
          uint8_t cr = ((ceilColor >> 16) & 0xFF) * ceilShade * 0.3;
          uint8_t cg = ((ceilColor >> 8) & 0xFF) * ceilShade * 0.3;
          uint8_t cb = (ceilColor & 0xFF) * ceilShade * 0.3;
          game->screenBuffer[(WINDOW_HEIGHT - y) * WINDOW_WIDTH + x]
              = (0xFF << 24) | (cr << 16) | (cg << 8) | cb;
        }
    }

  // Draw stamina bar
  int staminaWidth = 200;
  int staminaHeight = 20;
  int staminaX = WINDOW_WIDTH - staminaWidth - 20;
  int staminaY = 20;

  // Background
  for (int x = 0; x < staminaWidth; x++)
    {
      for (int y = 0; y < staminaHeight; y++)
        {
          game->screenBuffer[(staminaY + y) * WINDOW_WIDTH + (staminaX + x)]
              = 0xFF333333;
        }
    }

  // Stamina level
  int currentStamina = (int)(game->player.stamina / 100.0 * staminaWidth);
  uint32_t staminaColor = game->player.sprinting ? 0xFFFF3333 : 0xFF33FF33;
  for (int x = 0; x < currentStamina; x++)
    {
      for (int y = 0; y < staminaHeight; y++)
        {
          game->screenBuffer[(staminaY + y) * WINDOW_WIDTH + (staminaX + x)]
              = staminaColor;
        }
    }

  // Draw minimap if enabled
  if (game->showMinimap)
    {
      renderMinimap (game);
    }

  // Update screen
  SDL_UpdateTexture (game->screenTexture, NULL, game->screenBuffer,
                     WINDOW_WIDTH * sizeof (uint32_t));
  SDL_RenderCopy (game->renderer, game->screenTexture, NULL, NULL);
  SDL_RenderPresent (game->renderer);
}

// Cleanup resources
void
cleanupGame (GameState *game)
{
  if (game)
    {
      if (game->screenBuffer)
        {
          free (game->screenBuffer);
          game->screenBuffer = NULL;
        }

      if (game->screenTexture)
        {
          SDL_DestroyTexture (game->screenTexture);
          game->screenTexture = NULL;
        }

      if (game->renderer)
        {
          SDL_DestroyRenderer (game->renderer);
          game->renderer = NULL;
        }

      if (game->window)
        {
          SDL_DestroyWindow (game->window);
          game->window = NULL;
        }

      SDL_Quit ();
    }

  if (game->footstepSound)
    {

      Mix_FreeChunk (game->footstepSound);
    }

  if (game->sprintSound)
    {

      Mix_FreeChunk (game->sprintSound);
    }

  if (game->backgroundMusic)
    {

      Mix_FreeMusic (game->backgroundMusic);
    }

  Mix_CloseAudio ();

  Mix_Quit ();
}

// Main function
int
main (int argc, char *argv[])
{
  GameState game = { 0 };

  if (!initGame (&game))
    {
      printf ("Failed to initialize game!\n");
      return 1;
    }

  while (game.running)
    {
      game.deltaTime = getDeltaTime (&game);
      handleInput (&game);
      updatePlayerStamina (&game);
      renderFrame (&game);
    }

  cleanupGame (&game);
  return 0;
}