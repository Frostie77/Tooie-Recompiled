#pragma once
namespace recomp::config { class Config; }
namespace recompui { class LauncherMenu; }
namespace tooie::updates {
void register_options(recomp::config::Config& general);
void launcher_created();
void launcher_updated(recompui::LauncherMenu*);
}
