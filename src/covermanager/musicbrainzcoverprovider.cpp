/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <algorithm>
#include <functional>

#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/application.h"
#include "core/closure.h"
#include "core/network.h"
#include "core/logging.h"
#include "albumcoverfetcher.h"
#include "coverprovider.h"
#include "musicbrainzcoverprovider.h"

const char *MusicbrainzCoverProvider::kReleaseSearchUrl = "https://musicbrainz.org/ws/2/release/";
const char *MusicbrainzCoverProvider::kAlbumCoverUrl = "https://coverartarchive.org/release/%1/front";
const int MusicbrainzCoverProvider::kLimit = 8;

MusicbrainzCoverProvider::MusicbrainzCoverProvider(Application *app, QObject *parent): CoverProvider("MusicBrainz", true, app, parent), network_(new NetworkAccessManager(this)) {}

bool MusicbrainzCoverProvider::StartSearch(const QString &artist, const QString &album, int id) {

  QString query = QString("release:\"%1\" AND artist:\"%2\"").arg(album.trimmed().replace('"', "\\\"")).arg(artist.trimmed().replace('"', "\\\""));

  QUrlQuery url_query;
  url_query.addQueryItem("query", query);
  url_query.addQueryItem("limit", QString::number(kLimit));
  url_query.addQueryItem("fmt", "json");

  QUrl url(kReleaseSearchUrl);
  url.setQuery(url_query);
  QNetworkRequest request(url);

  QNetworkReply *reply = network_->get(request);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply *, int)), reply, id);

  return true;

}

void MusicbrainzCoverProvider::CancelSearch(int id) {}

void MusicbrainzCoverProvider::HandleSearchReply(QNetworkReply *reply, int search_id) {

  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  if (!json_obj.contains("releases")) {
    if (json_obj.contains("error")) {
      QString error = json_obj["error"].toString();
      Error(error);
    }
    else {
      Error(QString("Json reply is missing releases."), json_obj);
    }
    emit SearchFinished(search_id, results);
    return;
  }
  QJsonValue json_value = json_obj["releases"];

  if (!json_value.isArray()) {
    Error("Json releases is not an array.", json_value);
    emit SearchFinished(search_id, results);
    return;
  }
  QJsonArray json_releases = json_value.toArray();

  if (json_releases.isEmpty()) {
    emit SearchFinished(search_id, results);
    return;
  }

  for (const QJsonValue &value : json_releases) {
    if (!value.isObject()) {
      Error("Invalid Json reply, album value is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();
    if (!json_obj.contains("id") || !json_obj.contains("title")) {
      Error("Invalid Json reply, album is missing id or title.", json_obj);
      continue;
    }
    QString id = json_obj["id"].toString();
    QString title = json_obj["title"].toString();
    CoverSearchResult result;
    QUrl url(QString(kAlbumCoverUrl).arg(id));
    result.description = title;
    result.image_url = url;
    results.append(result);
  }
  emit SearchFinished(search_id, results);

}

QByteArray MusicbrainzCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      QString failure_reason;
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error")) {
          failure_reason = json_obj["error"].toString();
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      Error(failure_reason);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject MusicbrainzCoverProvider::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server is missing Json data.", data);
    return QJsonObject();
  }
  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", json_doc);
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

void MusicbrainzCoverProvider::Error(QString error, QVariant debug) {
  qLog(Error) << "Musicbrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}
