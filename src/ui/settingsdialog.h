/* This file is part of Clementine.

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMap>

#include "config.h"

class GlobalShortcuts;
class LibraryDirectoryModel;
class OSDPretty;
class Ui_SettingsDialog;

#ifdef ENABLE_WIIMOTEDEV
  class WiimotedevShortcutsConfig;
#endif

class GstEngine;

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  SettingsDialog(QWidget* parent = 0);
  ~SettingsDialog();

  enum Page {
    Page_Playback = 0,
    Page_Behaviour,
    Page_GlobalShortcuts,
    Page_Notifications,
    Page_Library,
    Page_Lastfm,
    Page_Magnatune,
#ifdef ENABLE_WIIMOTEDEV
    Page_Wiimotedev
#endif
  };

  void SetLibraryDirectoryModel(LibraryDirectoryModel* model);
  void SetGlobalShortcutManager(GlobalShortcuts* manager);
  void SetGstEngine(const GstEngine* engine) { gst_engine_ = engine; }

  void OpenAtPage(Page page);

  // QDialog
  void accept();

  // QWidget
  void showEvent(QShowEvent* e);
  void hideEvent(QHideEvent *);

 private slots:
  void CurrentTextChanged(const QString& text);
  void NotificationTypeChanged();
  void LastFMValidationComplete(bool success);

  void PrettyOpacityChanged(int value);
  void PrettyColorPresetChanged(int index);
  void ChooseBgColor();
  void ChooseFgColor();

  void UpdatePopupVisible();
  void ShowTrayIconToggled(bool on);
  void GstPluginChanged(int index);
  void FadingOptionsChanged();
  void RgPreampChanged(int value);

 private:
#ifdef ENABLE_WIIMOTEDEV
  WiimotedevShortcutsConfig* wiimotedev_config_;
#endif
  const GstEngine* gst_engine_;

  Ui_SettingsDialog* ui_;

  bool loading_settings_;

  OSDPretty* pretty_popup_;

  QMap<QString, QString> language_map_;
};

#endif // SETTINGSDIALOG_H
