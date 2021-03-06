// Created by: Peter KURNEV
// Copyright (c) 2010-2014 OPEN CASCADE SAS
// Copyright (c) 2007-2010 CEA/DEN, EDF R&D, OPEN CASCADE
// Copyright (c) 2003-2007 OPEN CASCADE, EADS/CCR, LIP6, CEA/DEN, CEDRAT,
//                         EDF R&D, LEG, PRINCIPIA R&D, BUREAU VERITAS
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


#include <BOPAlgo_PaveFiller.hxx>
#include <BOPAlgo_SectionAttribute.hxx>
#include <BOPDS_Curve.hxx>
#include <BOPDS_DS.hxx>
#include <BOPDS_Iterator.hxx>
#include <BOPDS_PaveBlock.hxx>
#include <gp_Pnt.hxx>
#include <IntTools_Context.hxx>
#include <NCollection_BaseAllocator.hxx>
#include <Standard_ErrorHandler.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

//=======================================================================
//function : 
//purpose  : 
//=======================================================================
BOPAlgo_PaveFiller::BOPAlgo_PaveFiller()
:
  BOPAlgo_Algo()
{
  myDS=NULL;
  myIterator=NULL;
  myNonDestructive=Standard_False;
  myIsPrimary=Standard_True;
  myGlue=BOPAlgo_GlueOff;
}
//=======================================================================
//function : 
//purpose  : 
//=======================================================================
BOPAlgo_PaveFiller::BOPAlgo_PaveFiller
  (const Handle(NCollection_BaseAllocator)& theAllocator)
:
  BOPAlgo_Algo(theAllocator)
{
  myDS=NULL;
  myIterator=NULL;
  myNonDestructive=Standard_False;
  myIsPrimary=Standard_True;
  myGlue=BOPAlgo_GlueOff;
}
//=======================================================================
//function : ~
//purpose  : 
//=======================================================================
BOPAlgo_PaveFiller::~BOPAlgo_PaveFiller()
{
  Clear();
}
//=======================================================================
//function : SetNonDestructive
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::SetNonDestructive(const Standard_Boolean bFlag)
{
  myNonDestructive=bFlag;
}
//=======================================================================
//function : NonDestructive
//purpose  : 
//=======================================================================
Standard_Boolean BOPAlgo_PaveFiller::NonDestructive()const 
{
  return myNonDestructive;
}
//=======================================================================
//function : SetGlue
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::SetGlue(const BOPAlgo_GlueEnum theGlue)
{
  myGlue=theGlue;
}
//=======================================================================
//function : Glue
//purpose  : 
//=======================================================================
BOPAlgo_GlueEnum BOPAlgo_PaveFiller::Glue() const 
{
  return myGlue;
}
//=======================================================================
//function : SetIsPrimary
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::SetIsPrimary(const Standard_Boolean bFlag)
{
  myIsPrimary=bFlag;
}
//=======================================================================
//function : IsPrimary
//purpose  : 
//=======================================================================
Standard_Boolean BOPAlgo_PaveFiller::IsPrimary()const 
{
  return myIsPrimary;
}
//=======================================================================
//function : Clear
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::Clear()
{
  if (myIterator) {
    delete myIterator;
    myIterator=NULL;
  }
  if (myDS) {
    delete myDS;
    myDS=NULL;
  }
}
//=======================================================================
//function : DS
//purpose  : 
//=======================================================================
const BOPDS_DS& BOPAlgo_PaveFiller::DS()
{
  return *myDS;
}
//=======================================================================
//function : PDS
//purpose  : 
//=======================================================================
BOPDS_PDS BOPAlgo_PaveFiller::PDS()
{
  return myDS;
}
//=======================================================================
//function : Context
//purpose  : 
//=======================================================================
const Handle(IntTools_Context)& BOPAlgo_PaveFiller::Context()
{
  return myContext;
}
//=======================================================================
//function : SectionAttribute
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::SetSectionAttribute
  (const BOPAlgo_SectionAttribute& theSecAttr)
{
  mySectionAttribute = theSecAttr;
}
//=======================================================================
//function : SetArguments
//purpose  : 
//=======================================================================
void BOPAlgo_PaveFiller::SetArguments(const BOPCol_ListOfShape& theLS)
{
  myArguments=theLS;
}
//=======================================================================
//function : Arguments
//purpose  : 
//=======================================================================
const BOPCol_ListOfShape& BOPAlgo_PaveFiller::Arguments()const
{
  return myArguments;
}
//=======================================================================
// function: Init
// purpose: 
//=======================================================================
void BOPAlgo_PaveFiller::Init()
{
  myErrorStatus=0;
  //
  if (!myArguments.Extent()) {
    myErrorStatus=10;
    return;
  }
  //
  // 0 Clear
  Clear();
  //
  // 1.myDS 
  myDS=new BOPDS_DS(myAllocator);
  myDS->SetArguments(myArguments);
  myDS->Init(myFuzzyValue);
  //
  // 2.myIterator 
  myIterator=new BOPDS_Iterator(myAllocator);
  myIterator->SetRunParallel(myRunParallel);
  myIterator->SetDS(myDS);
  myIterator->Prepare();
  //
  // 3 myContext
  myContext=new IntTools_Context;
  //
  // 4 NonDestructive flag
  SetNonDestructive();
  //
  myErrorStatus=0;
}
//=======================================================================
// function: Perform
// purpose: 
//=======================================================================
void BOPAlgo_PaveFiller::Perform()
{
  myErrorStatus=0;
  try { 
    OCC_CATCH_SIGNALS
    //
    PerformInternal();
  }
  //
  catch (Standard_Failure) {
    myErrorStatus=11;
  } 
}
//=======================================================================
// function: PerformInternal
// purpose: 
//=======================================================================
void BOPAlgo_PaveFiller::PerformInternal()
{
  myErrorStatus=0;
  //
  Init();
  if (myErrorStatus) {
    return; 
  }
  //
  Prepare();
  if (myErrorStatus) {
    return; 
  }
  // 00
  PerformVV();
  if (myErrorStatus) {
    return; 
  }
  // 01
  PerformVE();
  if (myErrorStatus) {
    return; 
  }
  //
  UpdatePaveBlocksWithSDVertices();
  myDS->UpdatePaveBlocks();
  // 11
  PerformEE();
  if (myErrorStatus) {
    return; 
  }
  UpdatePaveBlocksWithSDVertices();
  // 02
  PerformVF();
  if (myErrorStatus) {
    return; 
  }
  UpdatePaveBlocksWithSDVertices();
  // 12
  PerformEF();
  if (myErrorStatus) {
    return; 
  }
  UpdatePaveBlocksWithSDVertices();
  //
  // 22
  PerformFF();
  if (myErrorStatus) {
    return; 
  }
  //
  UpdateBlocksWithSharedVertices();
  //
  MakeSplitEdges();
  if (myErrorStatus) {
    return; 
  }
  //
  UpdatePaveBlocksWithSDVertices();
  //
  MakeBlocks();
  if (myErrorStatus) {
    return; 
  }
  //
  UpdateInterfsWithSDVertices();
  RefineFaceInfoOn();
  //
  MakePCurves();
  if (myErrorStatus) {
    return; 
  }
  //
  ProcessDE();
  if (myErrorStatus) {
    return; 
  }
}
