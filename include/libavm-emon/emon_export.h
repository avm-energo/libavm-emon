#pragma once

#include <QtCore/qglobal.h>

#if defined(EMON_EXPORTS)
#define AVM_EMON_EXPORT Q_DECL_EXPORT
#elif defined(EMON_IMPORTS)
#define AVM_EMON_EXPORT Q_DECL_IMPORT
#else
#define AVM_EMON_EXPORT
#endif
