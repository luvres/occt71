// Copyright (c) 2015 OPEN CASCADE SAS
//
// This file is part of Open CASCADE Technology software library.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published
// by the Free Software Foundation, with special exception defined in the file
// OCCT_LGPL_EXCEPTION.txt. Consult the file LICENSE_LGPL_21.txt included in OCCT
// distribution for complete text of the license and disclaimer of any warranty.
//
// Alternatively, this file may be used under the terms of Open CASCADE
// commercial license or contractual agreement.


#ifndef _StdLPersistent_Function_HeaderFile
#define _StdLPersistent_Function_HeaderFile

#include <StdObjMgt_Attribute.hxx>

#include <TFunction_Function.hxx>
#include <Standard_GUID.hxx>


class StdLPersistent_Function : public StdObjMgt_Attribute<TFunction_Function>
{
public:
  //! Read persistent data from a file.
  inline void Read (StdObjMgt_ReadData& theReadData)
    { theReadData >> myDriverGUID >> myFailure; }

  //! Import transient attribuite from the persistent data.
  void Import (const Handle(TFunction_Function)& theAttribute) const
  {
    theAttribute->SetDriverGUID (myDriverGUID);
    theAttribute->SetFailure    (myFailure);
  }

private:
  Standard_GUID    myDriverGUID;
  Standard_Integer myFailure;
};

#endif
