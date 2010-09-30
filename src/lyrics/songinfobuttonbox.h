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

#ifndef SONGINFOBUTTONBOX_H
#define SONGINFOBUTTONBOX_H

#include "core/song.h"
#include "widgets/slimbuttonbox.h"

class QSplitter;
class QStackedWidget;

class LyricFetcher;
class LyricView;

class SongInfoButtonBox : public SlimButtonBox {
  Q_OBJECT

public:
  SongInfoButtonBox(QWidget* parent = 0);

  static const char* kSettingsGroup;

  void SetSplitter(QSplitter* splitter);

  LyricFetcher* lyric_fetcher() const;
  LyricView* lyric_view() const { return lyrics_; }

public slots:
  void SongChanged(const Song& metadata);
  void SongFinished();

private slots:
  void SetActive(int index);
  void SplitterSizeChanged();

private:
  void UpdateCurrentSong();

private:
  QSplitter* splitter_;
  QStackedWidget* stack_;

  LyricView* lyrics_;

  Song metadata_;
};

#endif // SONGINFOBUTTONBOX_H