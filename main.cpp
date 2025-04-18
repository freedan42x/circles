#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifndef NO_DEBUG
#define ASSERT(expr) do {                                               \
    if (!(expr)) {                                                      \
      fprintf(stderr, "%s:%d: assertion %s failed\n", __FILE__, __LINE__, #expr); \
      abort();                                                          \
    }                                                                   \
  } while (0)
#else
#define ASSERT(...)
#endif

int random_int(int a, int b) {
  return a + rand() % (b - a);
}

Color random_color() {
  return ColorFromHSV(random_int(0, 361), 0.45f, 1.0f);
}

#define vec2s(x) (Vector2{x, x})

template <class T>
struct Array {  
  int size;
  int cap;
  T *items;

  void push(T e) {
    if (size >= cap) {
      cap = (cap + 2) * 1.5;
      items = (T *) realloc(items, sizeof(T) * cap);
    }
    items[size++] = e;
  }

  T& operator[](int ix) {
    ASSERT(ix >= 0 && ix < size);
    return items[ix];
  }

  T *begin() { return items; }
  T *end() { return items + size; }

  void remove_unordered(int ix) {
    ASSERT(ix >= 0 && ix < size);
    items[ix] = items[--size];
  }

  void remove_ordered(int ix) {
    ASSERT(ix >= 0 && ix < size);
    memmove(items + ix, items + ix + 1, (size - ix - 1) * sizeof(T));
    size--;
  }
};

template <class T>
void arr_extend_ptr(Array<const T*> &self, const Array<T> &arr) {
  if (self.size + arr.size >= self.cap) {
    self.cap = (self.cap + arr.size) * 1.5;
    self.items = (const T **) realloc(self.items, sizeof(T*) * self.cap);
  }
  for (int i = 0; i < arr.size; i++) {
    self.items[self.size + i] = &arr.items[i];
  }
  self.size += arr.size;
}

const Vector2 operator-(const Vector2 &a) {
  return {-a.x, -a.y};
}

#define FULLSCREEN

#ifdef FULLSCREEN
#define SCREEN_WIDTH 2560
#define SCREEN_HEIGHT 1440
#else
#define SCREEN_WIDTH (1920.0f / 2 + 300)
#define SCREEN_HEIGHT (1080)
#endif

struct PositionState {
  enum Mode {
    Linear,
    Exp
  } mode;
  Vector2 start_pos;
  Vector2 target_pos;
  float elapsed;
  float total;

  operator bool() {
    return elapsed < total;
  }

  Vector2 get() {
    float t = elapsed / total;

    Vector2 pos;

    if (mode == Linear) {
    } else if (mode == Exp) {
      float a = 0.15f;
      t = t * (1.0f - a + a * t);
    }
    pos = Vector2Lerp(start_pos, target_pos, t);
    
    return pos;
  }

  void update() {
    elapsed += GetFrameTime();
  }
};

#define MASS_ANIM_DURATION 0.2f
struct SmoothMass {
  float start_mass;
  float target_mass;
  float elapsed;
  float total;

  float get() {
    float t = elapsed / total;
    return Lerp(start_mass, target_mass, t);
  }

  void make_instant(float mass) {
    start_mass = target_mass;
    target_mass = mass;
    total = MASS_ANIM_DURATION;
    elapsed = total;
  }
  
  void update(float mass) {
    if (!FloatEquals(mass, target_mass)) {
      start_mass = get();
      target_mass = mass;
      elapsed = GetFrameTime();
      total = MASS_ANIM_DURATION;
    } else if (elapsed < total) {
      elapsed += GetFrameTime();
      if (elapsed > total) elapsed = total;
    }
  }
};

#define MAP_SIZE 20000
#define MAX_CELL_COUNT 256
#define CELL_MINIMUM_MASS 1000
#define PELLET_MASS 400

#define CELL_SPEED_FACTOR 2.0f
#define SPLIT_DURATION_FACTOR 1.1f
#define CAMERA_DELAY 0.2f

static inline float mass2speed(float mass) {
  return (150.0f - powf(mass, 0.35f)) * CELL_SPEED_FACTOR;
}

static inline float mass2split_time(float mass) {
  return (0.1f * logf(mass) - 0.25f) * SPLIT_DURATION_FACTOR;
}
static inline float mass2radius(float mass) {
  return 5.0f * sqrtf(mass / PI);
}

static const float pellet_radius = mass2radius(PELLET_MASS);

struct Cell {
  Vector2 pos;
  float mass;
  PositionState split_state;
  SmoothMass smooth_mass;

  inline float radius() const {
    return mass2radius(mass);
  }
};

struct CellView {
  Cell *cell;
  int player_ix;
};

struct Assets {
  Shader circle_mask;
  Shader circle_mask_outline;

  void load_shader(Shader &sh, const char *fs) {
    ASSERT(FileExists(fs));
    sh = LoadShader(0, fs);
    ASSERT(IsShaderValid(sh));
  }
};

Assets assets;

#define SKIN_SIZE 512
struct Skin {
  Texture2D texture;

  void load(const char *filepath, Shader sh=assets.circle_mask) {
    Texture2D skin_texture = LoadTexture(filepath);
    ASSERT(IsTextureValid(skin_texture));
    ASSERT(skin_texture.width == SKIN_SIZE && skin_texture.height == SKIN_SIZE);

    RenderTexture2D render_texture = LoadRenderTexture(SKIN_SIZE, SKIN_SIZE);

    BeginTextureMode(render_texture);
    BeginShaderMode(sh);
    DrawTexture(skin_texture, 0, 0, WHITE);
    EndShaderMode();
    EndTextureMode();

    UnloadTexture(skin_texture);

    Image img = LoadImageFromTexture(render_texture.texture);
    UnloadRenderTexture(render_texture);
    texture = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
  }

  void render(Vector2 pos, float r) {
    DrawTexturePro(texture,
                   {0, 0, (float)texture.width, (float)-texture.height},
                   {pos.x - r, pos.y - r, r * 2, r * 2},
                   {0, 0}, 0, WHITE);
  }
};

struct Player {
  bool ejecting;
  bool mouse_freeze;
  Skin skin;
  Color color;
  Array<Cell> cells;
  Vector2 hover_pos;
};

bool on_multi;
Array<Player> players;
Skin skin_outlines[2];
Vector2 camera_target;
int frame;

#define PLAYER_IX 0
#define MULTI_IX 1
#define PLAYER players[PLAYER_IX]
#define MULTI players[MULTI_IX]

struct Pellet {
  Color color;
  Vector2 pos;
  PositionState eject_state;
};

Array<Pellet> pellets;

void render_pellets() {
  for (auto p : pellets) {
    DrawCircleV(p.pos, pellet_radius, p.color);
  }
}

void render_cell_view(CellView view) {
  float r = mass2radius(view.cell->smooth_mass.get());

  Skin skin = players[view.player_ix].skin;
  if (IsTextureValid(skin.texture)) {
    int ix = on_multi ? MULTI_IX : PLAYER_IX;
    if (view.player_ix == ix) {
      skin_outlines[ix].render(view.cell->pos, r);
    } else {
      players[view.player_ix].skin.render(view.cell->pos, r);
    }
  } else {
    DrawCircleSector(view.cell->pos, r, 0, 360, 72, players[view.player_ix].color);
  }
}

void handle_border_collision(Vector2 &pos, float r) {
  if (pos.x < r) pos.x = r;
  if (pos.x + r > MAP_SIZE) pos.x = MAP_SIZE - r;
  if (pos.y < r) pos.y = r;
  if (pos.y + r > MAP_SIZE) pos.y = MAP_SIZE - r;
}

void handle_pellet_collision(Cell &cell) {
  for (int i = 0; i < pellets.size; i++) {
    handle_border_collision(pellets[i].pos, pellet_radius);
    if (Vector2DistanceSqr(cell.pos, pellets[i].pos) <= cell.radius() * cell.radius()) {
      cell.mass += PELLET_MASS;
      pellets.remove_unordered(i);
    }
  }
}

void handle_cell_collision(Cell &cell, Player &p) {
  if (!cell.split_state) {
    for (auto& c : p.cells) {
      if (&c == &cell) continue;
      if (c.split_state) continue;
      float overlap = c.radius() + cell.radius() - Vector2Distance(c.pos, cell.pos);
      if (overlap > 0) {
        Vector2 push_direction = Vector2Normalize(cell.pos - c.pos);
        cell.pos += push_direction * (overlap / 2) * GetFrameTime();
        c.pos -= push_direction * (overlap / 2) * GetFrameTime();
      }
    }
  }

  handle_border_collision(cell.pos, cell.radius());
}

void handle_enemy_collision(Cell &cell, Player &p) {
  for (auto &enemy : players) {
    if (&enemy == &p) continue;
    for (int i = 0; i < enemy.cells.size;) {
      Cell c = enemy.cells[i];
      float dst = Vector2DistanceSqr(cell.pos, c.pos);
      if ((cell.mass > c.mass*1.2f) && (dst <= cell.radius() * cell.radius())) {
        enemy.cells.remove_ordered(i);
        cell.mass += c.mass;
      } else {
        i++;
      }
    }
  }
}

Vector2 get_random_position(float spawn_mass) {
  float r = mass2radius(spawn_mass);
  float x = random_int(r, MAP_SIZE - r);
  float y = random_int(r, MAP_SIZE - r);
  return {x, y};
}

Vector2 get_position_near(Player &neighbor, float spawn_mass) {
  if (neighbor.cells.size <= 0) {
    Vector2 pos = get_random_position(spawn_mass);
    camera_target = pos;
    return pos;
  }
  
  float r = mass2radius(spawn_mass);
  Vector2 sum_pos{};
  float sum_mass = 0.0f;
  for (const auto &cell : neighbor.cells) {
    sum_mass += cell.mass;
    sum_pos += cell.pos;
  }
  float total_r = mass2radius(sum_mass);
  Vector2 center = sum_pos / neighbor.cells.size;

  int min_overlap = 1e9;
  Vector2 min_pos = get_random_position(spawn_mass);

  int tries = 20;
  int tries_inner = 20;
  int r_offset = 10;

  for (int i = 0; i < tries; i++) {
    for (int j = 0; j < tries_inner; j++) {
      float x = random_int(-r_offset, r_offset);
      float y = random_int(-r_offset, r_offset);
      Vector2 v = {x, y};
      Vector2 pos = center + Vector2Normalize(v) * (total_r + r + r_offset);

      handle_border_collision(pos, r);
      bool found = true;
      for (const auto &cell : neighbor.cells) {
        float overlap = r + cell.radius() - Vector2Distance(pos, cell.pos);
        if (overlap < min_overlap) {
          min_overlap = overlap;
          min_pos = pos;
        }
        if (overlap > 0) {
          found = false;
          break;
        }
      }
      if (found) return pos;
    }
    r_offset += 10;
  }

  return min_pos;
}

void eject_mass(Cell &cell, Player &p) {
  if (frame % 4 != 0) return;
  if (cell.mass - PELLET_MASS < CELL_MINIMUM_MASS) return;

  Vector2 direction = Vector2Normalize(p.hover_pos - cell.pos);
  Vector2 pos = cell.pos + direction * (cell.radius() + 10);
  Pellet pellet = {
    .color = random_color(),
    .pos = pos,
    .eject_state = {
      .mode = PositionState::Linear,
      .start_pos = pos,
      .target_pos = cell.pos + direction * (cell.radius() + 500),
      .elapsed = 0.0f,
      .total = 0.5f
    }
  };
  pellets.push(pellet);
  cell.mass -= PELLET_MASS;
}

void update_cell(Cell &cell, Player &p) {
  if (p.ejecting) eject_mass(cell, p);

  cell.smooth_mass.update(cell.mass);

  Vector2 speed = vec2s(mass2speed(cell.mass));
  Vector2 direction = Vector2Normalize(p.hover_pos - cell.pos);
  Vector2 pos_offset = direction * speed * GetFrameTime();
  PositionState &s = cell.split_state;
  if (s) {
    s.target_pos += pos_offset;
    cell.pos = s.get();
    s.update();
  } else {
    cell.pos += pos_offset;
  }
  
  handle_cell_collision(cell, p);
  handle_pellet_collision(cell);
  handle_enemy_collision(cell, p);
}

void split_cell(Cell &cell, Player &p) {
  if (p.cells.size >= MAX_CELL_COUNT || cell.mass/2 < CELL_MINIMUM_MASS) return;
  
  if (cell.split_state) cell.split_state = {};
        
  Vector2 direction = Vector2Normalize(p.hover_pos - cell.pos);
  cell.mass /= 2;

  Cell new_cell{};
  new_cell.pos = cell.pos + direction * cell.radius() / 4;
  new_cell.mass = cell.mass;
  new_cell.smooth_mass.make_instant(cell.mass);

  new_cell.split_state = {
    .mode = PositionState::Exp,
    .start_pos = new_cell.pos,
    .target_pos = cell.pos + direction * cell.radius() * 2,
    .elapsed = 0.0f,
    .total = mass2split_time(cell.mass * 2)
  };

  p.cells.push(new_cell);
}

void split(Player &p) {
  int size = p.cells.size; // important
  for (int i = 0; i < size; i++) split_cell(p.cells[i], p);
}

#define PLAYER_SKIN "skins/amr1.png"
#define MULTI_SKIN "skins/dex24.png"

void reset_map() {
  on_multi = false;
  players = {};
  
  Cell initial_cell{};
  initial_cell.pos = {MAP_SIZE / 2.0f - 2500, MAP_SIZE / 2.0f};
  initial_cell.mass = 400000;
  initial_cell.smooth_mass.make_instant(initial_cell.mass);
  camera_target = initial_cell.pos;

  Player player{};
  player.color = random_color();
  player.skin.load(PLAYER_SKIN);
  player.cells.size = 0;
  player.cells.push(initial_cell);
  players.push(player);
  
  Player multi{};
  multi.color = random_color();
  multi.skin.load(MULTI_SKIN);

  multi.cells.size = 0;
  multi.cells.push(initial_cell);
  multi.cells[0].pos.x += 2500;
  players.push(multi);

  pellets.size = 0;
}

Player &current_tab() {
  return on_multi ? MULTI : PLAYER;
}

Player &other_tab() {
  return on_multi ? PLAYER : MULTI;
}

int main() {
  srand(time(NULL) ^ getpid());

  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, " circles ");
#ifndef FULLSCREEN
  SetWindowPosition(1920/2 + 320, 200);
#endif
  #ifdef FULLSCREEN
  SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
#endif

  assets.load_shader(assets.circle_mask, "circle_mask.fs");
  assets.load_shader(assets.circle_mask_outline, "circle_mask_outline.fs");

  skin_outlines[PLAYER_IX].load(PLAYER_SKIN, assets.circle_mask_outline);
  skin_outlines[MULTI_IX].load(MULTI_SKIN, assets.circle_mask_outline);

  reset_map();
  
  Camera2D camera{};
  camera.target = PLAYER.cells[0].pos;
  camera.offset = { SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f };
  camera.rotation = 0.0f;
  camera.zoom = 0.1f;

  camera_target = camera.target;

  Array<CellView> sorted_cells{};
  
  while (!WindowShouldClose()) {
    frame++;

    DrawFPS(0, 0);
    DrawText(TextFormat("%d", current_tab().cells.size), 10, 10, 40, BLUE);
    if (current_tab().mouse_freeze) DrawText("Freeze", 80, 10, 40, RED);
    
    if (!current_tab().mouse_freeze) {
      current_tab().hover_pos = GetScreenToWorld2D(GetMousePosition(), camera);
    }
    
    if (IsKeyPressed(KEY_SPACE)) {
      split(current_tab());
    }

    if (IsKeyPressed(KEY_FOUR)) {
      for (int i = 0; i < 2; i++) split(current_tab());
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
      for (int i = 0; i < 3; i++) split(current_tab());
    }

    if (IsKeyPressed(KEY_B)) {
      for (int i = 0; i < 4; i++) split(current_tab());
    }

    current_tab().ejecting = IsKeyDown(KEY_W);

    if (IsKeyPressed(KEY_LEFT_SHIFT)) {
      on_multi = !on_multi;
    }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      current_tab().mouse_freeze = !current_tab().mouse_freeze;
    }

    if (IsKeyPressed(KEY_R)) {
      reset_map();
    }
    
    camera.zoom = expf(logf(camera.zoom) + ((float)GetMouseWheelMove()*0.1f));

    if (camera.zoom > 1.5f) camera.zoom = 1.5f;
    else if (camera.zoom < 0.05f) camera.zoom = 0.05f;

    for (auto &p : pellets) {
      PositionState &s = p.eject_state;
      if (p.eject_state) {
        p.pos = s.get();
        s.update();
      }
    }

    for (auto &tab : players) {    
      for (auto &cell : tab.cells) {
        update_cell(cell, tab);
      }
    }
    
    if (current_tab().cells.size <= 0 || IsKeyPressed(KEY_T)) {
      current_tab().cells.size = 0;
      current_tab().color = random_color();
      Cell cell{};
      cell.mass = 400000;
      cell.smooth_mass.make_instant(cell.mass);
      cell.pos = get_position_near(other_tab(), cell.mass);
      current_tab().cells.push(cell);
    }

    Array<Cell> &cells = current_tab().cells;
    Cell max_cell = cells[0];
    for (int i = 1; i < cells.size; i++) {
      if (cells[i].mass > max_cell.mass) max_cell = cells[i];
    }
    camera_target += (max_cell.pos - camera_target) * GetFrameTime() * CAMERA_DELAY;
    camera.target = camera_target;

    BeginDrawing();        
    ClearBackground(GetColor(0x181818FF));

    BeginMode2D(camera);
    DrawRectangleLinesEx({0, 0, MAP_SIZE, MAP_SIZE}, 40, PURPLE);

    render_pellets();

    // TODO: make something smarter instead of sorting each frame.
    // TODO: if mass is changing a little (like feeding each other), it jitters if overlapping
    sorted_cells.size = 0;
    for (int i = 0; i < players.size; i++) {
      for (auto &cell : players[i].cells) {
        sorted_cells.push({
            .cell = &cell,
            .player_ix = i
          });
      }
    }
    
    qsort(sorted_cells.items, sorted_cells.size, sizeof(*sorted_cells.items), [](const void *a, const void *b){
      return ((CellView *) b)->cell->mass < ((CellView *) a)->cell->mass ? 1 : -1;
    });

    for (auto &view : sorted_cells) {
      render_cell_view(view);
    }

    EndMode2D();
    EndDrawing();
  }

  
  CloseWindow();

  return 0;
}
