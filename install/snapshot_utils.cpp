
/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <libsnapshot/snapshot.h>

#include "recovery_ui/device.h"
#include "recovery_ui/ui.h"
#include "recovery_utils/roots.h"

using android::snapshot::CreateResult;
using android::snapshot::SnapshotManager;

bool FinishPendingSnapshotMerges(Device* device, bool called_from_wipe) {
  if (!android::base::GetBoolProperty("ro.virtual_ab.enabled", false)) {
    return true;
  }

  RecoveryUI* ui = device->GetUI();
  auto sm = SnapshotManager::New();
  if (!sm) {
    ui->Print("Could not create SnapshotManager.\n");
    return false;
  }

  if (!called_from_wipe) {
    using namespace android::snapshot;

    double progress;
    UpdateState update_state = sm->GetUpdateState(&progress);

    ui->Print("State of pending update: ");

    switch (update_state) {
      case UpdateState::None:
        ui->Print("Pending update not found\n");
        // HandleImminentDataWipe() is not a no-op even in this case, do not return
        break;
      case UpdateState::Initiated:
        ui->Print("Initiated\n");
        break;
      case UpdateState::Unverified:
        ui->Print("Unverified\n");
        break;
      case UpdateState::Merging:
        ui->Print("Merging, progress: %.2ff\n", progress);
        break;
      case UpdateState::MergeNeedsReboot:
        ui->Print("MergeNeedsReboot\n");
        break;
      case UpdateState::MergeCompleted:
        ui->Print("MergeCompleted\n");
        break;
      case UpdateState::MergeFailed:
        ui->Print("MergeFailed\n");
        break;
      case UpdateState::Cancelled:
        ui->Print("Cancelled\n");
        break;
      default:
        ui->Print("unknown (%i)\n", (int) update_state);
    }
  }

  auto callback = [&]() -> void {
    double progress;
    sm->GetUpdateState(&progress);
    ui->Print("Waiting for merge to complete: %.2f\n", progress);
  };
  if (!sm->HandleImminentDataWipe(callback)) {
    ui->Print("Unable to check merge status and/or complete update merge.\n");
    return false;
  }
  if (!called_from_wipe) {
    ui->Print("Operation completed successfully\n");
  }
  return true;
}

bool CreateSnapshotPartitions() {
  if (!android::base::GetBoolProperty("ro.virtual_ab.enabled", false)) {
    // If the device does not support Virtual A/B, there's no need to create
    // snapshot devices.
    return true;
  }

  auto sm = SnapshotManager::New();
  if (!sm) {
    // SnapshotManager could not be created. The device is still in a
    // consistent state and can continue with the mounting of the existing
    // devices, but cannot initialize snapshot devices.
    LOG(WARNING) << "Could not create SnapshotManager";
    return true;
  }

  auto ret = sm->RecoveryCreateSnapshotDevices();
  if (ret == CreateResult::ERROR) {
    return false;
  }
  return true;
}
