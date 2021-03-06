/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <test/bootstrapfixture.hxx>
#include <unotest/macros_test.hxx>
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <svx/svdpage.hxx>
#include <vcl/keycodes.hxx>
#include <vcl/scheduler.hxx>

#include <comphelper/propertysequence.hxx>
#include <com/sun/star/awt/Key.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <dbfunc.hxx>
#include <document.hxx>
#include <docuno.hxx>
#include <docsh.hxx>
#include <drwlayer.hxx>
#include <inputopt.hxx>
#include <rangeutl.hxx>
#include <scmod.hxx>
#include <tabvwsh.hxx>
#include <viewdata.hxx>

using namespace ::com::sun::star;

class ScUiCalcTest : public test::BootstrapFixture, public unotest::MacrosTest
{
public:
    virtual void setUp() override;

    virtual void tearDown() override;

    ScModelObj* createDoc(const char* pName);

protected:
    uno::Reference<lang::XComponent> mxComponent;
};

void ScUiCalcTest::setUp()
{
    test::BootstrapFixture::setUp();

    mxDesktop.set(frame::Desktop::create(mxComponentContext));
}

void ScUiCalcTest::tearDown()
{
    if (mxComponent.is())
        mxComponent->dispose();

    test::BootstrapFixture::tearDown();
}

static void lcl_AssertCurrentCursorPosition(SCCOL nCol, SCROW nRow)
{
    CPPUNIT_ASSERT_EQUAL(sal_Int16(nCol), ScDocShell::GetViewData()->GetCurX());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(nRow), ScDocShell::GetViewData()->GetCurY());
}

static void lcl_SelectRangeFromString(const ScDocument& rDoc, const OUString& rStr)
{
    ScRange aRange;
    sal_Int32 nOffset = 0;
    ScRangeStringConverter::GetRangeFromString(aRange, rStr, rDoc,
                                               formula::FormulaGrammar::CONV_OOO, nOffset);

    ScDocShell::GetViewData()->GetMarkData().SetMarkArea(aRange);
}

static void lcl_SelectObjectByName(std::u16string_view rObjName)
{
    ScTabViewShell* pViewShell = ScDocShell::GetViewData()->GetViewShell();
    CPPUNIT_ASSERT(pViewShell);

    bool bFound = pViewShell->SelectObject(rObjName);
    CPPUNIT_ASSERT_MESSAGE(
        OString(OUStringToOString(rObjName, RTL_TEXTENCODING_UTF8) + " not found.").getStr(),
        bFound);

    CPPUNIT_ASSERT(ScDocShell::GetViewData()->GetScDrawView()->AreObjectsMarked());
}

constexpr OUStringLiteral DATA_DIRECTORY = u"/sc/qa/unit/uicalc/data/";

ScModelObj* ScUiCalcTest::createDoc(const char* pName)
{
    if (mxComponent.is())
        mxComponent->dispose();
    mxComponent = loadFromDesktop(m_directories.getURLFromSrc(DATA_DIRECTORY)
                                      + OUString::createFromAscii(pName),
                                  "com.sun.star.sheet.SpreadsheetDocument");
    ScModelObj* pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    return pModelObj;
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf122232)
{
    ScModelObj* pModelObj = createDoc("tdf122232.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    //Start with from C6. Press tabulator to reach G6.
    lcl_AssertCurrentCursorPosition(2, 5);

    pModelObj->postKeyEvent(LOK_KEYEVENT_KEYINPUT, 0, KEY_TAB);
    pModelObj->postKeyEvent(LOK_KEYEVENT_KEYINPUT, 0, KEY_TAB);
    Scheduler::ProcessEventsToIdle();
    lcl_AssertCurrentCursorPosition(6, 5);

    //without the fix, cursor would jump to C29 instead of C7.
    pModelObj->postKeyEvent(LOK_KEYEVENT_KEYINPUT, 0, awt::Key::RETURN);
    Scheduler::ProcessEventsToIdle();
    lcl_AssertCurrentCursorPosition(2, 6);
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf120660)
{
    ScModelObj* pModelObj = createDoc("tdf120660.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    // Disable replace cell warning
    ScModule* pMod = SC_MOD();
    ScInputOptions aInputOption = pMod->GetInputOptions();
    bool bOldStatus = aInputOption.GetReplaceCellsWarn();
    aInputOption.SetReplaceCellsWarn(false);
    pMod->SetInputOptions(aInputOption);

    lcl_SelectRangeFromString(*pDoc, "A8:E8");

    ScDocument aClipDoc(SCDOCMODE_CLIP);
    ScDocShell::GetViewData()->GetView()->CopyToClip(&aClipDoc, false, false, false, false);
    Scheduler::ProcessEventsToIdle();

    lcl_SelectRangeFromString(*pDoc, "A4:E4");

    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(1200.0, pDoc->GetValue(ScAddress(4, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(-100.0, pDoc->GetValue(ScAddress(4, 7, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(2200.0, pDoc->GetValue(ScAddress(4, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(900.0, pDoc->GetValue(ScAddress(4, 7, 0)));

    lcl_SelectRangeFromString(*pDoc, "A8:D8");

    ScDocShell::GetViewData()->GetView()->CopyToClip(&aClipDoc, false, false, false, false);
    Scheduler::ProcessEventsToIdle();

    lcl_SelectRangeFromString(*pDoc, "A4:D4");

    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(1200.0, pDoc->GetValue(ScAddress(4, 3, 0)));

    // Without the fix in place, this test would have failed with
    // - Expected: -100
    // - Actual  : 900
    CPPUNIT_ASSERT_EQUAL(-100.0, pDoc->GetValue(ScAddress(4, 7, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(2200.0, pDoc->GetValue(ScAddress(4, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(900.0, pDoc->GetValue(ScAddress(4, 7, 0)));

    // Restore previous status
    aInputOption.SetReplaceCellsWarn(bOldStatus);
    pMod->SetInputOptions(aInputOption);
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf138710)
{
    ScModelObj* pModelObj = createDoc("tdf138710.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    dispatchCommand(mxComponent, ".uno:SelectAll", {});

    // Without the fix in place, this test would have crashed here
    dispatchCommand(mxComponent, ".uno:DeleteRows", {});

    CPPUNIT_ASSERT_EQUAL(OUString(""), pDoc->GetString(ScAddress(0, 0, 1)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(OUString("Total"), pDoc->GetString(ScAddress(0, 0, 1)));

    dispatchCommand(mxComponent, ".uno:DeleteRows", {});

    CPPUNIT_ASSERT_EQUAL(OUString(""), pDoc->GetString(ScAddress(0, 0, 1)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(OUString("Total"), pDoc->GetString(ScAddress(0, 0, 1)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf133326)
{
    ScModelObj* pModelObj = createDoc("tdf133326.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    dispatchCommand(mxComponent, ".uno:SelectAll", {});

    // .uno:Copy without touching shared clipboard
    ScDocument aClipDoc(SCDOCMODE_CLIP);
    ScDocShell::GetViewData()->GetView()->CopyToClip(&aClipDoc, false, false, false, false);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(1), pDoc->GetTableCount());

    uno::Sequence<beans::PropertyValue> aArgs(comphelper::InitPropertySequence(
        { { "Name", uno::Any(OUString("")) }, { "Index", uno::Any(sal_Int32(2)) } }));
    dispatchCommand(mxComponent, ".uno:Insert", aArgs);

    OUString aFormula;
    pDoc->GetFormula(0, 0, 1, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(2), pDoc->GetTableCount());

    // .uno:Paste without touching shared clipboard
    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(0, 0, 1, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=RAND()*1000000"), aFormula);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(2), pDoc->GetTableCount());

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(0, 0, 1, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(2), pDoc->GetTableCount());

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(1), pDoc->GetTableCount());

    dispatchCommand(mxComponent, ".uno:Redo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(2), pDoc->GetTableCount());
    pDoc->GetFormula(0, 0, 1, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);

    // Without the fix in place, it would have crashed here
    dispatchCommand(mxComponent, ".uno:Redo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(0, 0, 1, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=RAND()*1000000"), aFormula);
    CPPUNIT_ASSERT_EQUAL(static_cast<SCTAB>(2), pDoc->GetTableCount());
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf131455)
{
    ScModelObj* pModelObj = createDoc("tdf131455.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_AssertCurrentCursorPosition(0, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(1, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(4, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(5, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(8, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(9, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(12, 4);

    //Cursor can't move forward to the right
    for (size_t i = 0; i < 5; ++i)
    {
        dispatchCommand(mxComponent, ".uno:GoRight", {});
        lcl_AssertCurrentCursorPosition(13, 4);
    }

    CPPUNIT_ASSERT_EQUAL(sal_Int16(0), ScDocShell::GetViewData()->GetTabNo());

    dispatchCommand(mxComponent, ".uno:JumpToNextTable", {});

    CPPUNIT_ASSERT_EQUAL(sal_Int16(1), ScDocShell::GetViewData()->GetTabNo());
    lcl_AssertCurrentCursorPosition(0, 3);

    // Go to row 9
    for (size_t i = 0; i < 6; ++i)
    {
        dispatchCommand(mxComponent, ".uno:GoDown", {});
    }

    lcl_AssertCurrentCursorPosition(0, 9);

    dispatchCommand(mxComponent, ".uno:SelectRow", {});
    dispatchCommand(mxComponent, ".uno:DeleteRows", {});

    dispatchCommand(mxComponent, ".uno:JumpToPrevTable", {});

    CPPUNIT_ASSERT_EQUAL(sal_Int16(0), ScDocShell::GetViewData()->GetTabNo());
    lcl_AssertCurrentCursorPosition(13, 4);

    // Cursor can't move forward to the right
    // Without the fix in place, this test would have failed with
    // - Expected: 13
    // - Actual  : 64
    for (size_t i = 0; i < 5; ++i)
    {
        dispatchCommand(mxComponent, ".uno:GoRight", {});
        lcl_AssertCurrentCursorPosition(13, 4);
    }
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf126904)
{
    ScModelObj* pModelObj = createDoc("tdf126904.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_AssertCurrentCursorPosition(0, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(1, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(4, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(5, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(8, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(9, 4);
    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(12, 4);

    //Cursor can't move forward to the right
    for (size_t i = 0; i < 5; ++i)
    {
        dispatchCommand(mxComponent, ".uno:GoRight", {});
        lcl_AssertCurrentCursorPosition(13, 4);
    }
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf124816)
{
    ScModelObj* pModelObj = createDoc("tdf124816.xlsx");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    // The actual result is completely unrelated to this test and behaviour of
    // OFFSET() was changed as of tdf#85551 and here result of that test
    // document is now Err:502 instead of 0.
    const OUString aExpectedResult("Err:502");
    lcl_AssertCurrentCursorPosition(3, 9);
    CPPUNIT_ASSERT_EQUAL(aExpectedResult, pDoc->GetString(ScAddress(3, 9, 0)));

    //Without the fix, it would crash
    dispatchCommand(mxComponent, ".uno:InsertRowsBefore", {});
    CPPUNIT_ASSERT_EQUAL(OUString(""), pDoc->GetString(ScAddress(3, 9, 0)));
    dispatchCommand(mxComponent, ".uno:Undo", {});
    CPPUNIT_ASSERT_EQUAL(aExpectedResult, pDoc->GetString(ScAddress(3, 9, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf124815)
{
    ScModelObj* pModelObj = createDoc("tdf124815.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_AssertCurrentCursorPosition(0, 0);
    CPPUNIT_ASSERT_EQUAL(OUString("Rakennukset"), pDoc->GetString(ScAddress(2, 0, 0)));

    //Without the fix, it would crash
    dispatchCommand(mxComponent, ".uno:InsertColumnsBefore", {});
    CPPUNIT_ASSERT_EQUAL(OUString("Rakennukset"), pDoc->GetString(ScAddress(3, 0, 0)));
    dispatchCommand(mxComponent, ".uno:Undo", {});
    CPPUNIT_ASSERT_EQUAL(OUString("Rakennukset"), pDoc->GetString(ScAddress(2, 0, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf132431)
{
    ScModelObj* pModelObj = createDoc("tdf132431.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    OUString aFormula;
    pDoc->GetFormula(7, 219, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=SUMIFS($H$2:$H$198,B$2:B$198,G220)"), aFormula);
    CPPUNIT_ASSERT_EQUAL(0.0, pDoc->GetValue(ScAddress(7, 219, 0)));

    // Without the fix in place, it would crash here with
    // uncaught exception of type std::exception (or derived).
    // - vector::_M_fill_insert
    pDoc->SetString(ScAddress(7, 219, 0), "=SUMIFS($H$2:$DB$198,B$2:B$198,G220)");

    pDoc->GetFormula(7, 219, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=SUMIFS($H$2:$DB$198,B$2:B$198,G220)"), aFormula);
    CPPUNIT_ASSERT_EQUAL(0.0, pDoc->GetValue(ScAddress(7, 219, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString("Err:502"), pDoc->GetString(ScAddress(7, 219, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf83901)
{
    mxComponent = loadFromDesktop("private:factory/scalc");
    ScModelObj* pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_AssertCurrentCursorPosition(0, 0);
    pDoc->SetString(ScAddress(0, 1, 0), "=ROW(A3)");
    CPPUNIT_ASSERT_EQUAL(3.0, pDoc->GetValue(ScAddress(0, 1, 0)));

    dispatchCommand(mxComponent, ".uno:GoDown", {});
    dispatchCommand(mxComponent, ".uno:GoDown", {});
    lcl_AssertCurrentCursorPosition(0, 2);
    dispatchCommand(mxComponent, ".uno:SelectRow", {});
    dispatchCommand(mxComponent, ".uno:InsertRowsBefore", {});

    //Without the fix, it would be 3.0
    CPPUNIT_ASSERT_EQUAL(4.0, pDoc->GetValue(ScAddress(0, 1, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});
    CPPUNIT_ASSERT_EQUAL(3.0, pDoc->GetValue(ScAddress(0, 1, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf124822)
{
    ScModelObj* pModelObj = createDoc("tdf124822.xls");

    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    CPPUNIT_ASSERT_EQUAL(OUString("X"), pDoc->GetString(ScAddress(0, 0, 2)));

    dispatchCommand(mxComponent, ".uno:SelectAll", {});
    Scheduler::ProcessEventsToIdle();

    ScDocShell::GetViewData()->GetView()->CutToClip();

    CPPUNIT_ASSERT_EQUAL(OUString(""), pDoc->GetString(ScAddress(0, 0, 2)));

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_EQUAL(OUString("X"), pDoc->GetString(ScAddress(0, 0, 2)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf118189)
{
    ScModelObj* pModelObj = createDoc("tdf118189.xlsx");

    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    // Select column A
    lcl_SelectRangeFromString(*pDoc, "A:A");

    ScDocument aClipDoc(SCDOCMODE_CLIP);
    ScDocShell::GetViewData()->GetView()->CopyToClip(&aClipDoc, false, false, false, false);

    mxComponent->dispose();

    // Open a new document
    mxComponent = loadFromDesktop("private:factory/scalc");
    pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    OUString aFormula;
    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    ScDocShell::GetViewData()->GetView()->CutToClip();

    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);

    // Without the fix in place, this test would have crashed here
    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf118207)
{
    ScModelObj* pModelObj = createDoc("tdf118189.xlsx");

    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    // Disable replace cell warning
    ScModule* pMod = SC_MOD();
    ScInputOptions aInputOption = pMod->GetInputOptions();
    bool bOldStatus = aInputOption.GetReplaceCellsWarn();
    aInputOption.SetReplaceCellsWarn(false);
    pMod->SetInputOptions(aInputOption);

    // Select column A
    lcl_SelectRangeFromString(*pDoc, "A:A");

    OUString aFormula;
    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    ScDocument aClipDoc(SCDOCMODE_CLIP);
    ScDocShell::GetViewData()->GetView()->CutToClip(&aClipDoc);

    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);

    // Select column B
    lcl_SelectRangeFromString(*pDoc, "B:B");

    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(1, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(1, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(1, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(1, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);
    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString(""), aFormula);

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    pDoc->GetFormula(0, 77, 0, aFormula);
    CPPUNIT_ASSERT_EQUAL(OUString("=FALSE()"), aFormula);

    // Restore previous status
    aInputOption.SetReplaceCellsWarn(bOldStatus);
    pMod->SetInputOptions(aInputOption);
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf138428)
{
    mxComponent = loadFromDesktop("private:factory/scalc");
    ScModelObj* pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_AssertCurrentCursorPosition(0, 0);

    // Add a new comment
    uno::Sequence<beans::PropertyValue> aArgs
        = comphelper::InitPropertySequence({ { "Text", uno::makeAny(OUString("Comment")) } });
    dispatchCommand(mxComponent, ".uno:InsertAnnotation", aArgs);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There should be a note on A1", pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There shouldn't be a note on B1", !pDoc->HasNote(ScAddress(1, 0, 0)));

    // .uno:Copy without touching shared clipboard
    ScDocument aClipDoc(SCDOCMODE_CLIP);
    ScDocShell::GetViewData()->GetView()->CopyToClip(&aClipDoc, false, false, false, false);
    Scheduler::ProcessEventsToIdle();

    dispatchCommand(mxComponent, ".uno:GoRight", {});
    lcl_AssertCurrentCursorPosition(1, 0);

    // .uno:Paste without touching shared clipboard
    ScDocShell::GetViewData()->GetView()->PasteFromClip(InsertDeleteFlags::ALL, &aClipDoc);
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There should be a note on A1", pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There should be a note on B1", pDoc->HasNote(ScAddress(1, 0, 0)));

    // Without the fix in place, this test would have crashed here
    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There should be a note on A1", pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There shouldn't be a note on B1", !pDoc->HasNote(ScAddress(1, 0, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There shouldn't be a note on A1", !pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There shouldn't be a note on B1", !pDoc->HasNote(ScAddress(1, 0, 0)));

    dispatchCommand(mxComponent, ".uno:Redo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There should be a note on A1", pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There shouldn't be a note on B1", !pDoc->HasNote(ScAddress(1, 0, 0)));

    dispatchCommand(mxComponent, ".uno:Redo", {});
    Scheduler::ProcessEventsToIdle();

    CPPUNIT_ASSERT_MESSAGE("There should be a note on A1", pDoc->HasNote(ScAddress(0, 0, 0)));
    CPPUNIT_ASSERT_MESSAGE("There should be a note on B1", pDoc->HasNote(ScAddress(1, 0, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf130614)
{
    ScModelObj* pModelObj = createDoc("tdf130614.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    lcl_SelectObjectByName(u"Object 1");

    // FIXME: The OLE object is not copied/pasted if using CopyToClip/PasteToClip
    dispatchCommand(mxComponent, ".uno:Copy", {});
    Scheduler::ProcessEventsToIdle();

    mxComponent->dispose();

    // Open a new document
    mxComponent = loadFromDesktop("private:factory/scalc");
    pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    // Without the fix in place, this test would have crashed here
    dispatchCommand(mxComponent, ".uno:Paste", {});
    Scheduler::ProcessEventsToIdle();

    ScDrawLayer* pDrawLayer = pDoc->GetDrawLayer();
    SdrPage* pPage = pDrawLayer->GetPage(0);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), pPage->GetObjCount());
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf133342)
{
    ScModelObj* pModelObj = createDoc("tdf133342.ods");
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    CPPUNIT_ASSERT_EQUAL(OUString("12,35 %"), pDoc->GetString(ScAddress(0, 0, 0)));
    //Add decimals
    dispatchCommand(mxComponent, ".uno:NumberFormatIncDecimals", {});
    //Space should preserved before percent sign
    CPPUNIT_ASSERT_EQUAL(OUString("12,346 %"), pDoc->GetString(ScAddress(0, 0, 0)));

    //Delete decimals
    dispatchCommand(mxComponent, ".uno:NumberFormatDecDecimals", {});
    dispatchCommand(mxComponent, ".uno:NumberFormatDecDecimals", {});
    dispatchCommand(mxComponent, ".uno:NumberFormatDecDecimals", {});
    //Space should preserved before percent sign
    CPPUNIT_ASSERT_EQUAL(OUString("12 %"), pDoc->GetString(ScAddress(0, 0, 0)));

    dispatchCommand(mxComponent, ".uno:NumberFormatDecDecimals", {});
    //Space should preserved before percent sign
    CPPUNIT_ASSERT_EQUAL(OUString("12 %"), pDoc->GetString(ScAddress(0, 0, 0)));
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf71339)
{
    mxComponent = loadFromDesktop("private:factory/scalc");
    ScModelObj* pModelObj = dynamic_cast<ScModelObj*>(mxComponent.get());
    CPPUNIT_ASSERT(pModelObj);
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    pDoc->SetString(ScAddress(0, 1, 0), "1");
    pDoc->SetString(ScAddress(0, 2, 0), "1");

    lcl_SelectRangeFromString(*pDoc, "A1:A3");

    dispatchCommand(mxComponent, ".uno:AutoSum", {});

    CPPUNIT_ASSERT_EQUAL(2.0, pDoc->GetValue(ScAddress(0, 3, 0)));

    OUString aFormula;
    pDoc->GetFormula(0, 3, 0, aFormula);

    // Without the fix in place, this test would have failed with
    // - Expected: =SUM(A1:A3)
    // - Actual  : =SUM(A2:A3)
    CPPUNIT_ASSERT_EQUAL(OUString("=SUM(A1:A3)"), aFormula);
}

CPPUNIT_TEST_FIXTURE(ScUiCalcTest, testTdf81351)
{
    ScModelObj* pModelObj = createDoc("tdf81351.ods");
    CPPUNIT_ASSERT(pModelObj);
    ScDocument* pDoc = pModelObj->GetDocument();
    CPPUNIT_ASSERT(pDoc);

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    // Go to A1 before selecting A1:F5
    ScDocShell::GetViewData()->SetCurX(0);
    ScDocShell::GetViewData()->SetCurY(0);
    lcl_AssertCurrentCursorPosition(0, 0);

    lcl_SelectRangeFromString(*pDoc, "A1:F5");

    dispatchCommand(mxComponent, ".uno:SortAscending", {});
    dispatchCommand(mxComponent, ".uno:GoDown", {});

    // Without the fix in place, this test would have crashed
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    dispatchCommand(mxComponent, ".uno:Redo", {});

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    lcl_SelectRangeFromString(*pDoc, "A1:F5");

    dispatchCommand(mxComponent, ".uno:SortDescending", {});
    dispatchCommand(mxComponent, ".uno:GoDown", {});

    // Without the fix in place, this test would have crashed
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 4, 0)));

    dispatchCommand(mxComponent, ".uno:Undo", {});

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 4, 0)));

    dispatchCommand(mxComponent, ".uno:Redo", {});

    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Undo"), pDoc->GetString(ScAddress(0, 1, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Save"), pDoc->GetString(ScAddress(0, 2, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Paste"), pDoc->GetString(ScAddress(0, 3, 0)));
    CPPUNIT_ASSERT_EQUAL(OUString(".uno:Bold"), pDoc->GetString(ScAddress(0, 4, 0)));
}

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
