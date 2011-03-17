/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

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

#include "config.h"
#include "osd.h"
#include "osdpretty.h"
#include "ui/systemtrayicon.h"

#ifdef HAVE_DBUS
# include "dbus/notification.h"
#endif

#include <QCoreApplication>
#include <QtDebug>
#include <QSettings>

const char* OSD::kSettingsGroup = "OSD";

OSD::OSD(SystemTrayIcon* tray_icon, QObject* parent)
  : QObject(parent),
    tray_icon_(tray_icon),
    timeout_msec_(5000),
    behaviour_(Native),
    show_on_volume_change_(false),
    show_art_(true),
    show_on_play_mode_change_(true),
    force_show_next_(false),
    ignore_next_stopped_(false),
    pretty_popup_(new OSDPretty(OSDPretty::Mode_Popup)),
    cover_loader_(new BackgroundThreadImplementation<AlbumCoverLoader, AlbumCoverLoader>(this))
{
  cover_loader_->Start();

  connect(cover_loader_, SIGNAL(Initialised()), SLOT(CoverLoaderInitialised()));

  ReloadSettings();
  Init();
}

OSD::~OSD() {
  delete pretty_popup_;
}

void OSD::CoverLoaderInitialised() {
  cover_loader_->Worker()->SetPadOutputImage(false);
  cover_loader_->Worker()->SetDefaultOutputImage(QImage(":nocover.png"));
  connect(cover_loader_->Worker().get(), SIGNAL(ImageLoaded(quint64,QImage)),
          SLOT(AlbumArtLoaded(quint64,QImage)));
}

void OSD::ReloadSettings() {
  QSettings s;
  s.beginGroup(kSettingsGroup);
  behaviour_ = OSD::Behaviour(s.value("Behaviour", Native).toInt());
  timeout_msec_ = s.value("Timeout", 5000).toInt();
  show_on_volume_change_ = s.value("ShowOnVolumeChange", false).toBool();
  show_art_ = s.value("ShowArt", true).toBool();
  show_on_play_mode_change_ = s.value("ShowOnPlayModeChange", true).toBool();

  if (!SupportsNativeNotifications() && behaviour_ == Native)
    behaviour_ = Pretty;
  if (!SupportsTrayPopups() && behaviour_ == TrayPopup)
    behaviour_ = Disabled;

  pretty_popup_->set_popup_duration(timeout_msec_);
  pretty_popup_->ReloadSettings();
}

void OSD::SongChanged(const Song &song) {
  // no cover art yet
  tray_icon_->SetNowPlaying(song, NULL);
  QString summary(song.PrettyTitle());
  if (!song.artist().isEmpty())
    summary = QString("%1 - %2").arg(song.artist(), summary);

  QStringList message_parts;
  if (!song.album().isEmpty())
    message_parts << song.album();
  if (song.disc() > 0)
    message_parts << tr("disc %1").arg(song.disc());
  if (song.track() > 0)
    message_parts << tr("track %1").arg(song.track());

  WaitingForAlbumArt waiting;
  waiting.icon = "notification-audio-play";
  waiting.summary = summary;
  waiting.message = message_parts.join(", ");

  if (show_art_) {
    // Load the art in a background thread (maybe from a remote server),
    // AlbumArtLoaded gets called when it's ready.
    quint64 id = cover_loader_->Worker()->LoadImageAsync(song);
    waiting_for_album_art_.insert(id, waiting);
  } else {
    AlbumArtLoaded(waiting, QImage());
  }
}

void OSD::CoverArtPathReady(const Song& song, const QString& image_path) {
  tray_icon_->SetNowPlaying(song, image_path);
}

void OSD::AlbumArtLoaded(quint64 id, const QImage& image) {
  WaitingForAlbumArt info = waiting_for_album_art_.take(id);
  AlbumArtLoaded(info, image);
}

void OSD::AlbumArtLoaded(const WaitingForAlbumArt info, const QImage& image) {
  ShowMessage(info.summary, info.message, info.icon, image);
}

void OSD::Paused() {
  ShowMessage(QCoreApplication::applicationName(), tr("Paused"));
}

void OSD::Stopped() {
  tray_icon_->ClearNowPlaying();
  if (ignore_next_stopped_) {
    ignore_next_stopped_ = false;
    return;
  }

  ShowMessage(QCoreApplication::applicationName(), tr("Stopped"));
}

void OSD::PlaylistFinished() {
  // We get a PlaylistFinished followed by a Stopped from the player
  ignore_next_stopped_ = true;

  ShowMessage(QCoreApplication::applicationName(), tr("Playlist finished"));
}

void OSD::VolumeChanged(int value) {
  if (!show_on_volume_change_)
    return;

  ShowMessage(QCoreApplication::applicationName(), tr("Volume %1%").arg(value));
}

void OSD::MagnatuneDownloadFinished(const QStringList& albums) {
  QString message;
  if (albums.count() == 1)
    message = albums[0];
  else
    message = tr("%1 albums").arg(albums.count());

  ShowMessage(tr("Magnatune download finished"), message, QString(),
              QImage(":/providers/magnatune.png"));
}

void OSD::ShowMessage(const QString& summary,
                      const QString& message,
                      const QString& icon,
                      const QImage& image) {
  switch (behaviour_) {
    case Native:
      if (image.isNull()) {
        ShowMessageNative(summary, message, icon, QImage());
      } else {
        ShowMessageNative(summary, message, QString(), image);
      }
      break;

#ifndef Q_OS_DARWIN
    case TrayPopup:
      tray_icon_->ShowPopup(summary, message, timeout_msec_);
      break;
#endif

    case Disabled:
      if (!force_show_next_)
        break;
      force_show_next_ = false;
      // fallthrough
    case Pretty:
      pretty_popup_->SetMessage(summary, message, image);
      pretty_popup_->show();
      break;

    default:
      break;
  }
}

#ifndef HAVE_DBUS
void OSD::CallFinished(QDBusPendingCallWatcher*) {}
#endif

#ifdef HAVE_WIIMOTEDEV

void OSD::WiiremoteActived(int id) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: actived").arg(QString::number(id)));
}

void OSD::WiiremoteDeactived(int id) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: disactived").arg(QString::number(id)));
}

void OSD::WiiremoteConnected(int id) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: connected").arg(QString::number(id)));
}

void OSD::WiiremoteDisconnected(int id) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: disconnected").arg(QString::number(id)));
}

void OSD::WiiremoteLowBattery(int id, int live) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: low battery (%2%)").arg(QString::number(id), QString::number(live)));
}

void OSD::WiiremoteCriticalBattery(int id, int live) {
  ShowMessage(QString(tr("%1: Wiimotedev module")).arg(QCoreApplication::applicationName()),
              tr("Wii Remote %1: critical battery (%2%) ").arg(QString::number(id), QString::number(live)));
}

#endif

void OSD::ShuffleModeChanged(PlaylistSequence::ShuffleMode mode) {
  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::Shuffle_Off:   current_mode = tr("Don't shuffle");   break;
      case PlaylistSequence::Shuffle_All:   current_mode = tr("Shuffle all");   break;
      case PlaylistSequence::Shuffle_Album: current_mode = tr("Shuffle by album"); break;
    }
    ShowMessage(QCoreApplication::applicationName(), current_mode);
  }
}

void OSD::RepeatModeChanged(PlaylistSequence::RepeatMode mode) {
  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::Repeat_Off:      current_mode = tr("Don't repeat");   break;
      case PlaylistSequence::Repeat_Track:    current_mode = tr("Repeat track");   break;
      case PlaylistSequence::Repeat_Album:    current_mode = tr("Repeat album"); break;
      case PlaylistSequence::Repeat_Playlist: current_mode = tr("Repeat playlist"); break;
    }
    ShowMessage(QCoreApplication::applicationName(), current_mode);
  }
}
