// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/PropertiesDialog.h"

#include <memory>

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"

#include "DolphinQt/Config/FilesystemWidget.h"
#include "DolphinQt/Config/InfoWidget.h"
#include "DolphinQt/Config/VerifyWidget.h"
#include "DolphinQt/QtUtils/WrapInScrollArea.h"

#include "UICommon/GameFile.h"

PropertiesDialog::PropertiesDialog(QWidget* parent, const UICommon::GameFile& game)
    : StackedSettingsWindow{parent}, m_filepath(game.GetFilePath())
{
  setWindowTitle(QStringLiteral("%1: %2 - %3")
                     .arg(QString::fromStdString(game.GetFileName()),
                          QString::fromStdString(game.GetGameID()),
                          QString::fromStdString(game.GetLongName())));

  auto* const info = new InfoWidget(game);
  AddWrappedPane(info, tr("Info"));

  if (game.GetPlatform() != DiscIO::Platform::ELFOrDOL)
  {
    std::shared_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(game.GetFilePath());
    if (volume)
    {
      auto* const verify = new VerifyWidget(volume);
      AddPane(verify, tr("Verify"));

      if (DiscIO::IsDisc(game.GetPlatform()))
      {
        auto* const filesystem = new FilesystemWidget(volume);
        AddPane(filesystem, tr("Filesystem"));
      }
    }
  }

  OnDoneCreatingPanes();
}
