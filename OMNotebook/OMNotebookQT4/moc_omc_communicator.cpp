/****************************************************************************
** Meta object code from reading C++ file 'omc_communicator.hpp'
**
** Created: ti 24. jan 12:19:47 2006
**      by: The Qt Meta Object Compiler version 58 (Qt 4.0.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "omc_communicator.hpp"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'omc_communicator.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 58
#error "This file was generated using the moc from 4.0.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

static const uint qt_meta_data_OmcCommunicator[] = {

 // content:
       1,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets

       0        // eod
};

static const char qt_meta_stringdata_OmcCommunicator[] = {
    "OmcCommunicator\0"
};

const QMetaObject OmcCommunicator::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_OmcCommunicator,
      qt_meta_data_OmcCommunicator, 0 }
};

const QMetaObject *OmcCommunicator::metaObject() const
{
    return &staticMetaObject;
}

void *OmcCommunicator::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_OmcCommunicator))
	return static_cast<void*>(const_cast<OmcCommunicator*>(this));
    return QObject::qt_metacast(_clname);
}

int OmcCommunicator::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    return _id;
}