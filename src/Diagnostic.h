#pragma once

#include <QString>

struct DiagnosticReport
{
    QString summary;
    QString details;
};

DiagnosticReport runFullDiagnostic();
