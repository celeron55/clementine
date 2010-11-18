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

#ifndef WIZARDPLUGIN_H
#define WIZARDPLUGIN_H

#include <QObject>

#include "generator_fwd.h"

class LibraryBackend;

class QWizard;

namespace smart_playlists {

class WizardPlugin : public QObject {
  Q_OBJECT

public:
  WizardPlugin(LibraryBackend* library, QObject* parent);

  virtual QString name() const = 0;
  virtual QString description() const = 0;
  int start_page() const { return start_page_; }

  virtual GeneratorPtr CreateGenerator() const = 0;

  void Init(QWizard* wizard);

protected:
  virtual int CreatePages(QWizard* wizard) = 0;

  LibraryBackend* library_;

private:
  int start_page_;
};

} // namespace smart_playlists

#endif // WIZARDPLUGIN_H