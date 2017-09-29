/*
 * Copyright (C) 2017 Ispirata Srl
 *
 * This file is part of Astarte.
 * Astarte is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Astarte is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Astarte.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bsondocument.h"

#include <QtCore/QDebug>
#include <QtCore/QHash>

#if defined(__APPLE__)
  #include "apple_endian.h"
#else
  #include <endian.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TYPE_DOUBLE 0x01
#define TYPE_STRING 0x02
#define TYPE_DOCUMENT 0x03
#define TYPE_BINARY 0x05
#define TYPE_BOOLEAN 0x08
#define TYPE_DATETIME 0x09
#define TYPE_INT32 0x10
#define TYPE_INT64 0x12

namespace Util
{

static uint32_t read_uint32(const void *u)
{
    const unsigned char *b = (const unsigned char *) u;
    return le32toh(((uint32_t) b[0]) | (((uint32_t) b[1]) << 8) | (((uint32_t) b[2]) << 16) | (((uint32_t) b[3]) << 24));
}

static uint64_t read_uint64(const void *u)
{
    const unsigned char *b = (const unsigned char *) u;
    return le64toh((uint64_t) b[0] | ((uint64_t) b[1] << 8) | ((uint64_t) b[2] << 16) | ((uint64_t) b[3] << 24) | ((uint64_t) b[4] << 32) | ((uint64_t) b[5] << 40) | ((uint64_t) b[6] << 48) | ((uint64_t) b[7] << 56));
}

static unsigned int bson_next_item_offset(unsigned int offset, unsigned int keyLen, const void *document)
{
    const char *docBytes = (const char *) document;
    uint8_t elementType = (uint8_t) docBytes[offset];

    /* offset <- type (uint8_t) + key (const char *) + '\0' (char) */
    offset += 1 + keyLen + 1;

    switch (elementType) {
        case TYPE_STRING: {
            uint32_t stringLen = read_uint32(docBytes + offset);
            offset += stringLen + 4;
        }
        break;

        case TYPE_DOCUMENT: {
            uint32_t docLen = read_uint32(docBytes + offset);
            offset += docLen;
        }
        break;

        case TYPE_BINARY: {
            uint32_t binLen = read_uint32(docBytes + offset);
            offset += 4 + 1 + binLen; /* int32 (len) + byte (subtype) + binLen */
        }
        break;

        case TYPE_INT32: {
           offset += sizeof(int32_t);
        }
        break;

        case TYPE_DOUBLE:
        case TYPE_DATETIME:
        case TYPE_INT64: {
            offset += sizeof(int64_t);
        }
        break;

        case TYPE_BOOLEAN: {
            offset += 1;
        }
        break;

        default: {
            qWarning() << "BSON parser: unrecognized BSON type: " << elementType;
            return 0;
        }
    }

    return offset;
}

static const void *bson_key_lookup(const char *key, const void *document, uint8_t *type)
{
    const char *docBytes = (const char *) document;
    uint32_t docLen = read_uint32(document);

    /* TODO: it would be nice to check len validity here */

    unsigned int offset = 4;
    while (offset + 1 < docLen) {
       uint8_t elementType = (uint8_t) docBytes[offset];
       int keyLen = strnlen(docBytes + offset + 1, docLen - offset);

       if (!strncmp(key, docBytes + offset + 1, docLen - offset)) {
           if (type) {
               *type = elementType;
           }
           return (void *) (docBytes + offset + 1 + keyLen + 1);
       }

       unsigned int newOffset = bson_next_item_offset(offset, keyLen, document);
       if (!newOffset) {
           return NULL;
       }
       offset = newOffset;
    }

    return NULL;
}

static void *bson_next_item(const void *document, const void *current_item)
{
    const char *docBytes = (const char *) document;
    uint32_t docLen = read_uint32(document);
    unsigned int offset = ((const char *) current_item) - docBytes;

    if (offset + 1 >= docLen) {
        return NULL;
    }

    int keyLen = strnlen(docBytes + offset + 1, docLen - offset);
    unsigned int newOffset = bson_next_item_offset(offset, keyLen, document);

    if (!newOffset) {
        return NULL;
    }

    if (newOffset + 1 >= docLen) {
        return NULL;
    }

    return ((char *) docBytes) + newOffset;
}

static const void *bson_first_item(const void *document)
{
    const char *docBytes = (const char *) document;
    return docBytes + 4;
}

static const char *bson_key(const void *item)
{
    return ((const char *) item) + 1;
}

static const char *bson_value_to_string(const void *valuePtr, uint32_t *len)
{
    const char *valueBytes = (const char *) valuePtr;
    uint32_t stringLen = read_uint32(valueBytes);

    if (len) {
        *len = stringLen - 1;
    }

    return valueBytes + 4;
}

static const char *bson_value_to_binary(const void *valuePtr, uint32_t *len)
{
    const char *valueBytes = (const char *) valuePtr;
    uint32_t binLen = read_uint32(valueBytes);

    if (len) {
        *len = binLen;
    }

    return valueBytes + 5;
}

static const void *bson_value_to_document(const void *valuePtr, uint32_t *len)
{
    const char *valueBytes = (const char *) valuePtr;
    uint32_t binLen = read_uint32(valueBytes);

    if (len) {
        *len = binLen;
    }

    return valueBytes;
}

static int8_t bson_value_to_int8(const void *valuePtr)
{
    return ((int8_t *) valuePtr)[0];
}

static int32_t bson_value_to_int32(const void *valuePtr)
{
    return (int32_t) read_uint32(valuePtr);
}

static int64_t bson_value_to_int64(const void *valuePtr)
{
    return (int64_t) read_uint64(valuePtr);
}

static double bson_value_to_double(const void *valuePtr)
{
    union data64 {
        uint64_t u64value;
        double dvalue;
    } v;
    v.u64value = read_uint64(valuePtr);
    return v.dvalue;
}

static int bson_check_validity(const void *document, unsigned int fileSize)
{
    const char *docBytes = (const char *) document;
    uint32_t docLen = read_uint32(document);
    int offset;

    if (!fileSize) {
        qWarning() << "Empty buffer: no BSON document found";
        return 0;
    }

    if ((docLen == 5) && (fileSize >= 5) && (docBytes[4] == 0)) {
        // empty document
        return 1;
    }

    if (fileSize < 4 + 1 + 2 + 1) {
        qWarning() << "BSON data too small";
        return 0;
    }

    if (docLen > fileSize) {
        qWarning() << "BSON document is bigger than data: data: " << fileSize << " document: " << docLen;
        return 0;
    }

    if (docBytes[docLen - 1] != 0) {
        qWarning() << "BSON document is not terminated by null byte.";
        return 0;
    }

    offset = 4;
    switch (docBytes[offset]) {
       case TYPE_DOUBLE:
       case TYPE_STRING:
       case TYPE_DOCUMENT:
       case TYPE_BINARY:
       case TYPE_BOOLEAN:
       case TYPE_DATETIME:
       case TYPE_INT32:
       case TYPE_INT64:
       break;

       default:
           qWarning() << "Unrecognized BSON document first type\n";
           return 0;
    }

    return 1;
}

static int32_t bson_document_size(const void *document)
{
    return read_uint32(document);
}

BSONDocument::BSONDocument(const QByteArray &document)
    : m_doc(document)
{
}

int BSONDocument::size() const
{
    if (Q_LIKELY(m_doc.count() >= 4)) {
        return bson_document_size(m_doc.constData());
    }

    return 0;
}

bool BSONDocument::isValid() const
{
    return !m_doc.isEmpty() && bson_check_validity(m_doc.constData(), m_doc.count());
}

bool BSONDocument::contains(const char *name) const
{
    return bson_key_lookup(name, m_doc.constData(), 0);
}

QVariant BSONDocument::value(const char *name, QVariant defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_UNLIKELY(!value)) {
        return defaultValue;
    }

    switch (type) {
        case TYPE_DOUBLE:
            return QVariant(bson_value_to_double(value));

        case TYPE_STRING:
            return QVariant(QString::fromUtf8(bson_value_to_string(value, 0)));

        case TYPE_DOCUMENT: {
            uint32_t len = 0;
            const char *subdocumentData = (const char *) bson_value_to_document(value, &len);
            return QVariant(QByteArray(subdocumentData, len));
        }
        case TYPE_BINARY: {
            uint32_t len = 0;
            const char *data = bson_value_to_binary(value, &len);
            return QVariant(QByteArray(data, len));
        }
        case TYPE_BOOLEAN:
            return QVariant((bool) (bson_value_to_int8(value) == '\1'));

        case TYPE_DATETIME:
            return QVariant(QDateTime::fromMSecsSinceEpoch(bson_value_to_int64(value)).toLocalTime());

        case TYPE_INT32:
            return QVariant(bson_value_to_int32(value));

        case TYPE_INT64:
            return QVariant((qlonglong) bson_value_to_int64(value));

        default:
            return defaultValue;
    }
}

double BSONDocument::doubleValue(const char *name, double defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value)) {
        if (type == TYPE_DOUBLE) {
            return bson_value_to_double(value);

        } else if (type == TYPE_INT64) {
            return bson_value_to_int64(value);

        } else if (type == TYPE_INT32) {
            return bson_value_to_int32(value);
        }
    }

    return defaultValue;
}

QByteArray BSONDocument::byteArrayValue(const char *name, const QByteArray &defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    const char *data;
    if (value && (type == TYPE_STRING)) {
        data = bson_value_to_string(value, 0);
        return QByteArray(data);

    } else if (value && (type == TYPE_BINARY)) {
        uint32_t len = 0;
        data = bson_value_to_binary(value, &len);
        return QByteArray(data, len);
    }

    return defaultValue;
}

QString BSONDocument::stringValue(const char *name, const QString &defaultValue) const
{
    QByteArray encoded = byteArrayValue(name);
    if (encoded.isEmpty()) {
        return defaultValue;
    } else {
        return QString::fromUtf8(encoded);
    }
}

QDateTime BSONDocument::dateTimeValue(const char *name, const QDateTime &defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value && (type == TYPE_DATETIME))) {
        return QDateTime::fromMSecsSinceEpoch(bson_value_to_int64(value)).toLocalTime();
    }

    return defaultValue;
}

int32_t BSONDocument::int32Value(const char *name, int32_t defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value && (type == TYPE_INT32))) {
        return bson_value_to_int32(value);
    }

    return defaultValue;
}

int64_t BSONDocument::int64Value(const char *name, int64_t defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value)) {
        if (type == TYPE_INT64) {
            return bson_value_to_int64(value);
        } else if (type == TYPE_INT32) {
            return bson_value_to_int32(value);
        }
    }

    return defaultValue;
}

bool BSONDocument::booleanValue(const char *name, bool defaultValue) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value && (type == TYPE_BOOLEAN))) {
        return bson_value_to_int8(value) == '\1';
    }

    return defaultValue;
}

BSONDocument BSONDocument::subdocument(const char *name) const
{
    uint8_t type;
    const void *value = bson_key_lookup(name, m_doc.constData(), &type);

    if (Q_LIKELY(value && (type == TYPE_DOCUMENT))) {
        uint32_t len = 0;
        const char *subdocumentData = (const char *) bson_value_to_document(value, &len);
        if (len) {
            return BSONDocument(QByteArray(subdocumentData, len));
        }
    }

    return BSONDocument(QByteArray());
}

QHash<QByteArray, QByteArray> BSONDocument::byteArrayValuesHash() const
{
    QHash<QByteArray, QByteArray> tmp;

    for (const void *item = bson_first_item(m_doc.constData()); item != 0; item = bson_next_item(m_doc.constData(), item)) {
        tmp.insert(QByteArray(bson_key(item)), byteArrayValue(bson_key(item)));
    }

    return tmp;
}

QByteArray BSONDocument::toByteArray() const
{
    return m_doc;
}

} // Utils
