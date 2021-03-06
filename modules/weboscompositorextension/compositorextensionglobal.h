// Copyright (c) 2014-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef COMPOSITOREXTENSIONEXPORT_H
#define COMPOSITOREXTENSIONEXPORT_H

#include <QtCore/qglobal.h>

#if !defined(WEBOS_COMPOSITOR_EXT_EXPORT)
#  if defined(QT_SHARED)
#    define WEBOS_COMPOSITOR_EXT_EXPORT Q_DECL_EXPORT
#  else
#    define WEBOS_COMPOSITOR_EXT_EXPORT
#  endif
#endif

#endif //COMPOSITOREXTENSIONEXPORT_H
