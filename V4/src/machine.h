#pragma once
#include <QString>
namespace cms {
struct ElevationLaunch { void *process = nullptr; unsigned long error = 0; };
bool validMachineAction(const QString &action);
bool isProcessElevated();
ElevationLaunch launchMachineAction(const QString &action, bool french, void *parentWindow);
int runMachineAction(const QString &action, bool french);
}
