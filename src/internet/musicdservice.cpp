/* This file is part of Clementine.
   Copyright 2012, David Sansome <me@davidsansome.com>

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

#include "musicdservice.h"

#include <QDesktopServices>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <qjson/parser.h>
#include <qjson/serializer.h>

#include "internetmodel.h"
#include "searchboxwidget.h"

#include "core/application.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/mergedproxymodel.h"
#include "core/network.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "globalsearch/globalsearch.h"
#include "globalsearch/musicdsearchprovider.h"
#include "ui/iconloader.h"

const char* MusicdService::kServiceName = "Musicd";
const char* MusicdService::kSettingsGroup = "Musicd";
const char* MusicdService::kDefaultServerAddress = "http://localhost:6800";

const int MusicdService::kSearchDelayMsec = 400;
const int MusicdService::kSongSearchLimit = 100;
const int MusicdService::kSongSimpleSearchLimit = 10;

typedef QPair<QString, QString> Param;

MusicdService::MusicdService(Application* app, InternetModel *parent)
  : InternetService(kServiceName, app, parent, parent),
    root_(NULL),
    search_(NULL),
    network_(new NetworkAccessManager(this)),
    context_menu_(NULL),
    search_box_(new SearchBoxWidget(this)),
    search_delay_(new QTimer(this)),
    next_pending_search_id_(0) {

  search_delay_->setInterval(kSearchDelayMsec);
  search_delay_->setSingleShot(true);
  connect(search_delay_, SIGNAL(timeout()), SLOT(DoSearch()));

  MusicdSearchProvider* search_provider = new MusicdSearchProvider(app_, this);
  search_provider->Init(this);
  app_->global_search()->AddProvider(search_provider);

  connect(search_box_, SIGNAL(TextChanged(QString)), SLOT(Search(QString)));
}


MusicdService::~MusicdService() {
}

void MusicdService::ReloadSettings() {
  QSettings s;
  s.beginGroup(kSettingsGroup);

  server_address_ = s.value("server_address", kDefaultServerAddress).toString();
}

QStandardItem* MusicdService::CreateRootItem() {
  root_ = new QStandardItem(QIcon(":providers/musicd.png"), kServiceName);
  root_->setData(true, InternetModel::Role_CanLazyLoad);
  root_->setData(InternetModel::PlayBehaviour_DoubleClickAction,
                           InternetModel::Role_PlayBehaviour);
  return root_;
}

void MusicdService::LazyPopulate(QStandardItem* item) {
  switch (item->data(InternetModel::Role_Type).toInt()) {
    case InternetModel::Type_Service: {
      EnsureItemsCreated();
      break;
    }
    default:
      break;
  }
}

void MusicdService::EnsureItemsCreated() {
  search_ = new QStandardItem(IconLoader::Load("edit-find"),
                              tr("Search results"));
  search_->setToolTip(tr("Start typing something on the search box above to "
                         "fill this search results list"));
  search_->setData(InternetModel::PlayBehaviour_MultipleItems,
                   InternetModel::Role_PlayBehaviour);
  root_->appendRow(search_);
}

QWidget* MusicdService::HeaderWidget() const {
  return search_box_;
}

void MusicdService::Search(const QString& text, bool now) {
  pending_search_ = text;

  // If there is no text (e.g. user cleared search box), we don't need to do a
  // real query that will return nothing: we can clear the playlist now
  if (text.isEmpty()) {
    search_delay_->stop();
    ClearSearchResults();
    return;
  }

  if (now) {
    search_delay_->stop();
    DoSearch();
  } else {
    search_delay_->start();
  }
}

void MusicdService::DoSearch() {
  ClearSearchResults();

  QList<Param> parameters;
  parameters  << Param("search", pending_search_);
  QNetworkReply* reply = CreateRequest("tracks", parameters);
  const int id = next_pending_search_id_++;
  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(SearchFinished(QNetworkReply*,int)),
             reply, id);
}

void MusicdService::SearchFinished(QNetworkReply* reply, int task_id) {
  qLog(Debug) << "Musicd SearchFinished()";

  reply->deleteLater();

  SongList songs = ExtractSongs(ExtractResult(reply));
  // Fill results list
  foreach (const Song& song, songs) {
    QStandardItem* child = CreateSongItem(song);
    search_->appendRow(child);
  }

  QModelIndex index = model()->merged_model()->mapFromSource(search_->index());
  ScrollToIndex(index);
}

void MusicdService::ClearSearchResults() {
  if (search_)
    search_->removeRows(0, search_->rowCount());
}

int MusicdService::SimpleSearch(const QString& text) {
  QList<Param> parameters;
  parameters  << Param("search", text);
  QNetworkReply* reply = CreateRequest("tracks", parameters);
  const int id = next_pending_search_id_++;
  NewClosure(reply, SIGNAL(finished()),
             this, SLOT(SimpleSearchFinished(QNetworkReply*,int)),
             reply, id);
  return id;
}

void MusicdService::SimpleSearchFinished(QNetworkReply* reply, int id) {
  qLog(Debug) << "Musicd SimpleSearchFinished()";

  reply->deleteLater();

  SongList songs = ExtractSongs(ExtractResult(reply));
  emit SimpleSearchResults(id, songs);
}

void MusicdService::EnsureMenuCreated() {
  if(!context_menu_) {
    context_menu_ = new QMenu;
    context_menu_->addActions(GetPlaylistActions());
  }
}

void MusicdService::ShowContextMenu(const QPoint& global_pos) {
  EnsureMenuCreated();
  
  context_menu_->popup(global_pos);
}

QNetworkReply* MusicdService::CreateRequest(
    const QString& ressource_name,
    const QList<Param>& params) {

  QUrl url(server_address_);

  url.setPath(ressource_name);

  foreach(const Param& param, params) {
    url.addQueryItem(param.first, param.second);
  }

  qLog(Debug) << "Request Url: " << url.toEncoded();

  QNetworkRequest req(url);
  QNetworkReply *reply = network_->get(req);
  return reply;
}

QVariant MusicdService::ExtractResult(QNetworkReply* reply) {
  qLog(Debug) << "Musicd ExtractResult()";
  QJson::Parser parser;
  bool ok;
  QVariant result = parser.parse(reply, &ok);
  if (!ok) {
    qLog(Error) << "Error while parsing Musicd result";
  }
  return result;
}

SongList MusicdService::ExtractSongs(const QVariant& result) {
  qLog(Debug) << "Musicd ExtractSongs()";
  SongList songs;

  QVariantList q_variant_list = result.toMap()["tracks"].toList();
  qLog(Debug) << "Musicd ExtractSongs(): size:" << q_variant_list.size();
  foreach(const QVariant& q, q_variant_list) {
    Song song = ExtractSong(q.toMap());
    if (song.is_valid()) {
      songs << song;
    }
  }
  return songs;
}

Song MusicdService::ExtractSong(const QVariantMap& result_song) {
  qLog(Debug) << "Musicd ExtractSong(): " << result_song["id"].toString();
  Song song;
  if (!result_song.isEmpty()) {
  	QString id = result_song["id"].toString();
    QUrl stream_url(server_address_);
	stream_url.setPath("open");
	stream_url.addQueryItem("id", id);
    song.set_url(stream_url);

	QString artist = result_song["artist"].toString();
    song.set_artist(artist);

    QString title = result_song["title"].toString();
    song.set_title(title);

    QVariant q_duration = result_song["duration"];
    quint64 duration = q_duration.toULongLong() * kNsecPerSec;
    song.set_length_nanosec(duration);

    song.set_valid(true);
  }
  return song;
}
