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

#include "musicdsettingspage.h"

#include "core/network.h"
#include "musicdservice.h"
#include "internetmodel.h"
#include "ui_musicdsettingspage.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QtDebug>

MusicdSettingsPage::MusicdSettingsPage(SettingsDialog* dialog)
  : SettingsPage(dialog),
    network_(new NetworkAccessManager(this)),
    ui_(new Ui_MusicdSettingsPage)
{
  ui_->setupUi(this);
  setWindowIcon(QIcon(":/providers/musicd.png"));
}

MusicdSettingsPage::~MusicdSettingsPage() {
  delete ui_;
}

void MusicdSettingsPage::Load() {
  QSettings s;
  s.beginGroup(MusicdService::kSettingsGroup);

  ui_->server_address->setText(s.value("server_address",
      MusicdService::kDefaultServerAddress).toString());
}

void MusicdSettingsPage::Save() {
  QSettings s;
  s.beginGroup(MusicdService::kSettingsGroup);

  s.setValue("server_address", ui_->server_address->text());

  InternetModel::Service<MusicdService>()->ReloadSettings();
}

