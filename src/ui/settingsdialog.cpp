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

#include "appearancesettingspage.h"
#include "backgroundstreamssettingspage.h"
#include "behavioursettingspage.h"
#include "config.h"
#include "globalshortcutssettingspage.h"
#include "iconloader.h"
#include "playbacksettingspage.h"
#include "networkproxysettingspage.h"
#include "notificationssettingspage.h"
#include "mainwindow.h"
#include "settingsdialog.h"
#include "core/application.h"
#include "core/backgroundstreams.h"
#include "core/logging.h"
#include "core/networkproxyfactory.h"
#include "core/player.h"
#include "engines/enginebase.h"
#include "engines/gstengine.h"
#include "globalsearch/globalsearchsettingspage.h"
#include "internet/digitallyimportedsettingspage.h"
#include "internet/groovesharksettingspage.h"
#include "internet/magnatunesettingspage.h"
#include "internet/musicdsettingspage.h"
#include "library/librarysettingspage.h"
#include "playlist/playlistview.h"
#include "podcasts/podcastsettingspage.h"
#include "songinfo/songinfosettingspage.h"
#include "transcoder/transcodersettingspage.h"
#include "widgets/groupediconview.h"
#include "widgets/osdpretty.h"

#include "ui_settingsdialog.h"

#ifdef HAVE_LIBLASTFM
# include "internet/lastfmsettingspage.h"
#endif

#ifdef HAVE_WIIMOTEDEV
# include "wiimotedev/wiimotesettingspage.h"
#endif

#ifdef HAVE_SPOTIFY
# include "internet/spotifysettingspage.h"
#endif

#ifdef HAVE_GOOGLE_DRIVE
# include "internet/googledrivesettingspage.h"
#endif

#include <QDesktopWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>


SettingsItemDelegate::SettingsItemDelegate(QObject* parent)
  : QStyledItemDelegate(parent)
{
}

QSize SettingsItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const {
  const bool is_separator = index.data(SettingsDialog::Role_IsSeparator).toBool();
  QSize ret = QStyledItemDelegate::sizeHint(option, index);

  if (is_separator) {
    ret.setHeight(ret.height() * 2);
  }

  return ret;
}

void SettingsItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
  const bool is_separator = index.data(SettingsDialog::Role_IsSeparator).toBool();

  if (is_separator) {
    GroupedIconView::DrawHeader(painter, option.rect, option.font,
                                option.palette, index.data().toString());
  } else {
    QStyledItemDelegate::paint(painter, option, index);
  }
}


SettingsDialog::SettingsDialog(Application* app, BackgroundStreams* streams, QWidget* parent)
  : QDialog(parent),
    app_(app),
    model_(app_->library_model()->directory_model()),
    gst_engine_(qobject_cast<GstEngine*>(app_->player()->engine())),
    song_info_view_(NULL),
    streams_(streams),
    global_search_(app_->global_search()),
    appearance_(app_->appearance()),
    ui_(new Ui_SettingsDialog),
    loading_settings_(false)
{
  ui_->setupUi(this);
  ui_->list->setItemDelegate(new SettingsItemDelegate(this));

  QTreeWidgetItem* general = AddCategory(tr("General"));

  AddPage(Page_Playback, new PlaybackSettingsPage(this), general);
  AddPage(Page_Behaviour, new BehaviourSettingsPage(this), general);
  AddPage(Page_Library, new LibrarySettingsPage(this), general);
  AddPage(Page_Proxy, new NetworkProxySettingsPage(this), general);
  AddPage(Page_Transcoding, new TranscoderSettingsPage(this), general);

#ifdef HAVE_WIIMOTEDEV
  AddPage(Page_Wiimotedev, new WiimoteSettingsPage(this), general);
#endif

  // User interface
  QTreeWidgetItem* iface = AddCategory(tr("User interface"));
  AddPage(Page_GlobalShortcuts, new GlobalShortcutsSettingsPage(this), iface);
  AddPage(Page_GlobalSearch, new GlobalSearchSettingsPage(this), iface);
  AddPage(Page_Appearance, new AppearanceSettingsPage(this), iface);
  AddPage(Page_SongInformation, new SongInfoSettingsPage(this), iface);
  AddPage(Page_Notifications, new NotificationsSettingsPage(this), iface);

  // Internet providers
  QTreeWidgetItem* providers = AddCategory(tr("Internet providers"));

#ifdef HAVE_LIBLASTFM
  AddPage(Page_Lastfm, new LastFMSettingsPage(this), providers);
#endif

  AddPage(Page_Grooveshark, new GroovesharkSettingsPage(this), providers);

#ifdef HAVE_GOOGLE_DRIVE
  AddPage(Page_GoogleDrive, new GoogleDriveSettingsPage(this), providers);
#endif

#ifdef HAVE_SPOTIFY
  AddPage(Page_Spotify, new SpotifySettingsPage(this), providers);
#endif

  AddPage(Page_Magnatune, new MagnatuneSettingsPage(this), providers);
  AddPage(Page_DigitallyImported, new DigitallyImportedSettingsPage(this), providers);
  AddPage(Page_BackgroundStreams, new BackgroundStreamsSettingsPage(this), providers);
  AddPage(Page_Podcasts, new PodcastSettingsPage(this), providers);
  AddPage(Page_Musicd, new MusicdSettingsPage(this), providers);

  // List box
  connect(ui_->list, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
          SLOT(CurrentItemChanged(QTreeWidgetItem*)));
  ui_->list->setCurrentItem(pages_[Page_Playback].item_);

  // Make sure the list is big enough to show all the items
  ui_->list->setMinimumWidth(static_cast<QAbstractItemView*>(ui_->list)->sizeHintForColumn(0));

  ui_->buttonBox->button(QDialogButtonBox::Cancel)->setShortcut(QKeySequence::Close);
}

SettingsDialog::~SettingsDialog() {
  delete ui_;
}

QTreeWidgetItem* SettingsDialog::AddCategory(const QString& name) {
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText(0, name);
  item->setData(0, Role_IsSeparator, true);
  item->setFlags(Qt::ItemIsEnabled);

  ui_->list->invisibleRootItem()->addChild(item);
  item->setExpanded(true);

  return item;
}

void SettingsDialog::AddPage(Page id, SettingsPage* page, QTreeWidgetItem* parent) {
  if (!parent)
    parent = ui_->list->invisibleRootItem();

  // Connect page's signals to the settings dialog's signals
  connect(page, SIGNAL(NotificationPreview(OSD::Behaviour,QString,QString)),
                SIGNAL(NotificationPreview(OSD::Behaviour,QString,QString)));
  connect(page, SIGNAL(SetWiimotedevInterfaceActived(bool)),
                SIGNAL(SetWiimotedevInterfaceActived(bool)));

  // Create the list item
  QTreeWidgetItem* item = new QTreeWidgetItem;
  item->setText(0, page->windowTitle());
  item->setIcon(0, page->windowIcon());
  item->setData(0, Role_IsSeparator, false);

  if (!page->IsEnabled()) {
    item->setFlags(Qt::NoItemFlags);
  }

  parent->addChild(item);

  // Create a scroll area containing the page
  QScrollArea* area = new QScrollArea;
  area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  area->setWidget(page);
  area->setWidgetResizable(true);
  area->setFrameShape(QFrame::NoFrame);
  area->setMinimumWidth(page->layout()->minimumSize().width());

  // Add the page to the stack
  ui_->stacked_widget->addWidget(area);

  // Remember where the page is
  PageData data;
  data.item_ = item;
  data.scroll_area_ = area;
  data.page_ = page;
  pages_[id] = data;
}

void SettingsDialog::accept() {
  // Save settings
  foreach (const PageData& data, pages_.values()) {
    data.page_->Save();
  }

  QDialog::accept();
}

void SettingsDialog::reject() {
  // Notify each page that user clicks on Cancel
  foreach (const PageData& data, pages_.values()) {
    data.page_->Cancel();
  }

  QDialog::reject();
}

void SettingsDialog::showEvent(QShowEvent* e) {
  // Load settings
  loading_settings_ = true;
  foreach (const PageData& data, pages_.values()) {
    data.page_->Load();
  }
  loading_settings_ = false;

  // Resize the dialog if it's too big
  const QSize available = QApplication::desktop()->availableGeometry(this).size();
  if (available.height() < height()) {
    resize(width(), sizeHint().height());
  }

  QDialog::showEvent(e);
}

void SettingsDialog::OpenAtPage(Page page) {
  if (!pages_.contains(page)) {
    return;
  }

  ui_->list->setCurrentItem(pages_[page].item_);
  show();
}

void SettingsDialog::CurrentItemChanged(QTreeWidgetItem* item) {
  if (! (item->flags() & Qt::ItemIsSelectable)) {
    return;
  }

  // Set the title
  ui_->title->setText("<b>" + item->text(0) + "</b>");

  // Display the right page
  foreach (const PageData& data, pages_.values()) {
    if (data.item_ == item) {
      ui_->stacked_widget->setCurrentWidget(data.scroll_area_);
      break;
    }
  }
}
