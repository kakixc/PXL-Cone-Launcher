// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QRegularExpression>
#include <QStringList>

namespace Pxl {
inline bool oneSettingsSearchTypo(const QString& a, const QString& b)
{
    if (a.size() < 5 || b.size() < 5 || qAbs(a.size()-b.size()) > 1) return false;
    qsizetype first=0;
    while (first<a.size() && first<b.size() && a[first]==b[first]) ++first;
    if (first==qMin(a.size(),b.size())) return true;
    if (a.size()==b.size()) {
        if (a.mid(first+1)==b.mid(first+1)) return true;
        return first+1<a.size() && a[first]==b[first+1] && a[first+1]==b[first]
            && a.mid(first+2)==b.mid(first+2);
    }
    return a.size()>b.size() ? a.mid(first+1)==b.mid(first) : a.mid(first)==b.mid(first+1);
}
inline bool settingsSearchMatches(const QString& text, const QString& query)
{
    const auto content=text.toCaseFolded();
    const auto needle=query.trimmed().toCaseFolded();
    if (needle.isEmpty() || content.contains(needle)) return true;
    static const QRegularExpression separator(QStringLiteral("[^\\p{L}\\p{N}]+"));
    const auto terms=needle.split(separator,Qt::SkipEmptyParts);
    if (terms.isEmpty()) return false;
    const auto words=content.split(separator,Qt::SkipEmptyParts);
    for (const auto& term : terms) {
        bool found=false;
        for (const auto& word : words)
            if (word.startsWith(term) || oneSettingsSearchTypo(term,word)) { found=true; break; }
        if (!found) return false;
    }
    return true;
}
}
