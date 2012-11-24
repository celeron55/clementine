/* This file is part of Clementine.
   Copyright 2011, David Sansome <me@davidsansome.com>

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

#include "musicdsearchprovider.h"

#include <QIcon>

#include "core/application.h"
#include "core/logging.h"
#include "covers/albumcoverloader.h"
#include "internet/musicdservice.h"

MusicdSearchProvider::MusicdSearchProvider(Application* app, QObject* parent)
    : SearchProvider(app, parent),
      service_(NULL)
{
}

void MusicdSearchProvider::Init(MusicdService* service) {
  service_ = service;
  SearchProvider::Init("Musicd", "musicd",
                       QIcon(":providers/musicd.png"),
                       WantsDelayedQueries | ArtIsProbablyRemote | CanShowConfig);

  connect(service_, SIGNAL(SimpleSearchResults(int, SongList)),
          SLOT(SearchDone(int, SongList)));

  cover_loader_options_.desired_height_ = kArtHeight;
  cover_loader_options_.pad_output_image_ = true;
  cover_loader_options_.scale_output_image_ = true;

  connect(app_->album_cover_loader(),
          SIGNAL(ImageLoaded(quint64, QImage)),
          SLOT(AlbumArtLoaded(quint64, QImage)));
}

void MusicdSearchProvider::SearchAsync(int id, const QString& query) {
  const int service_id = service_->SimpleSearch(query);
  pending_searches_[service_id] = PendingState(id, TokenizeQuery(query));;
}

void MusicdSearchProvider::SearchDone(int id, const SongList& songs) {
  // Map back to the original id.
  const PendingState state = pending_searches_.take(id);
  const int global_search_id = state.orig_id_;

  ResultList ret;
  foreach (const Song& song, songs) {
    Result result(this);
    result.metadata_ = song;

    ret << result;
  }

  emit ResultsAvailable(global_search_id, ret);
  MaybeSearchFinished(global_search_id);
}

void MusicdSearchProvider::MaybeSearchFinished(int id) {
  if (pending_searches_.keys(PendingState(id, QStringList())).isEmpty()) {
    emit SearchFinished(id);
  }
}

void MusicdSearchProvider::LoadArtAsync(int id, const Result& result) {
  quint64 loader_id = app_->album_cover_loader()->LoadImageAsync(
        cover_loader_options_, result.metadata_);
  cover_loader_tasks_[loader_id] = id;
}

void MusicdSearchProvider::AlbumArtLoaded(quint64 id, const QImage& image) {
  if (!cover_loader_tasks_.contains(id)) {
    return;
  }
  int original_id = cover_loader_tasks_.take(id);
  emit ArtLoaded(original_id, image);
}
