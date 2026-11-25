// Hold'em Lab — desktop poker trainer.
//
// Developer flags (used for automated screenshots):
//   --frames N        quit after N frames
//   --screenshot F    save a screenshot to F just before quitting
//   --tab NAME        start on play|review|stats|drills|ranges|settings
//   --autoplay        the hero checks/calls automatically
//   --wait-hero       after N frames, keep running until it's the hero's turn
//   --drill K         open drill K (0-4) on the Drills tab; --answer answers it
//   --size WxH        initial window size

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "raylib.h"
#include "ui/app.hpp"

int main(int argc, char** argv) {
  int frames = -1, width = 1440, height = 900;
  std::string shot, tab;
  bool autoplay = false, waitHero = false, drillAnswer = false;
  int drill = -1;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
    else if (a == "--screenshot" && i + 1 < argc) shot = argv[++i];
    else if (a == "--tab" && i + 1 < argc) tab = argv[++i];
    else if (a == "--autoplay") autoplay = true;
    else if (a == "--wait-hero") waitHero = true;
    else if (a == "--drill" && i + 1 < argc) drill = std::atoi(argv[++i]);
    else if (a == "--answer") drillAnswer = true;
    else if (a == "--size" && i + 1 < argc) std::sscanf(argv[++i], "%dx%d", &width, &height);
  }

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
  SetTraceLogLevel(LOG_WARNING);
  InitWindow(width, height, "Hold'em Lab");
  SetWindowMinSize(1000, 680);
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  {
    hl::ui::App app;
    app.g.load();
    if (!tab.empty()) {
      static const char* names[] = {"play", "review", "stats", "drills", "ranges", "settings"};
      for (int i = 0; i < 6; ++i)
        if (tab == names[i]) app.tab = static_cast<hl::ui::Tab>(i);
    }
    app.ctrl.newGame();
    if (drill >= 0) {
      app.tab = hl::ui::Tab::Drills;
      app.drills().open(drill);
    }
    int n = 0;
    while (!WindowShouldClose()) {
      bool stopAutoplay = waitHero && frames > 0 && n >= frames;
      if (autoplay && !stopAutoplay && app.ctrl.st.awaitingHero) {
        hl::Legal l = app.ctrl.table->legal();
        app.ctrl.heroAct({l.toCall > 0 ? hl::Act::Call : hl::Act::Check, 0});
      }
      if (drillAnswer && n == 5) app.drills().debugAnswer(app);
      app.frame();
      ++n;
      bool done = frames > 0 && n >= frames && (!waitHero || (app.ctrl.st.awaitingHero && n >= frames + 30) || n > frames + 4000);
      if (done) {
        if (!shot.empty()) {
          Image img = LoadImageFromScreen();  // TakeScreenshot() would drop the directory part
          ExportImage(img, shot.c_str());
          UnloadImage(img);
        }
        break;
      }
    }
    app.settings.save();
    app.progress.save();
    app.ctrl.saveStats();
    app.g.unload();
  }
  CloseWindow();
  return 0;
}
