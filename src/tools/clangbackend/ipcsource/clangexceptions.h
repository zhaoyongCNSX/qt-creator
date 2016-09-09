/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include <clangbackendipc/filecontainer.h>

#include <utf8stringvector.h>

#include <clang-c/Index.h>

#include <exception>

namespace ClangBackEnd {

class ProjectPartDoNotExistException : public std::exception
{
public:
    ProjectPartDoNotExistException(const Utf8StringVector &projectPartIds);

    const Utf8StringVector &projectPartIds() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    Utf8StringVector projectPartIds_;
    mutable Utf8String what_;
};

class TranslationUnitAlreadyExistsException : public std::exception
{
public:
    TranslationUnitAlreadyExistsException(const FileContainer &fileContainer);
    TranslationUnitAlreadyExistsException(const Utf8String &filePath,
                                          const Utf8String &projectPartId);

    const FileContainer &fileContainer() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    FileContainer fileContainer_;
    mutable Utf8String what_;
};

class TranslationUnitDoesNotExistException : public std::exception
{
public:
    TranslationUnitDoesNotExistException(const FileContainer &fileContainer);
    TranslationUnitDoesNotExistException(const Utf8String &filePath,
                                         const Utf8String &projectPartId);

    const FileContainer &fileContainer() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    FileContainer fileContainer_;
    mutable Utf8String what_;
};

class TranslationUnitFileNotExitsException : public std::exception
{
public:
    TranslationUnitFileNotExitsException(const Utf8String &filePath);

    const Utf8String &filePath() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    Utf8String filePath_;
    mutable Utf8String what_;
};

class TranslationUnitIsNullException : public std::exception
{
public:
    const char *what() const Q_DECL_NOEXCEPT override;
};

class TranslationUnitParseErrorException : public std::exception
{
public:
    TranslationUnitParseErrorException(const Utf8String &filePath,
                                       const Utf8String &projectPartId,
                                       CXErrorCode errorCode);

    const Utf8String &filePath() const;
    const Utf8String &projectPartId() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    Utf8String filePath_;
    Utf8String projectPartId_;
    CXErrorCode errorCode_;
    mutable Utf8String what_;
};

class TranslationUnitReparseErrorException : public std::exception
{
public:
    TranslationUnitReparseErrorException(const Utf8String &filePath,
                                         const Utf8String &projectPartId,
                                         int errorCode);

    const Utf8String &filePath() const;
    const Utf8String &projectPartId() const;

    const char *what() const Q_DECL_NOEXCEPT override;

private:
    Utf8String filePath_;
    Utf8String projectPartId_;
    int errorCode_;
    mutable Utf8String what_;
};

} // namespace ClangBackEnd
