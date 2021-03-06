/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

#include <memory>

#include <svx/devtools/DevelopmentToolDockingWindow.hxx>

#include <com/sun/star/uno/XComponentContext.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>

#include <com/sun/star/beans/theIntrospection.hpp>
#include <com/sun/star/beans/XIntrospection.hpp>
#include <com/sun/star/beans/XIntrospectionAccess.hpp>
#include <com/sun/star/beans/Property.hpp>
#include <com/sun/star/beans/PropertyConcept.hpp>
#include <com/sun/star/beans/MethodConcept.hpp>
#include <com/sun/star/reflection/XIdlMethod.hpp>

#include <comphelper/processfactory.hxx>

#include <sfx2/dispatch.hxx>
#include <sfx2/sfxmodelfactory.hxx>
#include <svx/svxids.hrc>

#include <sfx2/objsh.hxx>

#include <sfx2/viewfrm.hxx>

#include <com/sun/star/frame/XController.hpp>
#include <com/sun/star/view/XSelectionChangeListener.hpp>

#include <cppuhelper/compbase.hxx>
#include <cppuhelper/basemutex.hxx>

#include <com/sun/star/view/XSelectionSupplier.hpp>

using namespace css;

namespace
{
typedef cppu::WeakComponentImplHelper<css::view::XSelectionChangeListener>
    SelectionChangeHandlerInterfaceBase;

class SelectionChangeHandler final : private ::cppu::BaseMutex,
                                     public SelectionChangeHandlerInterfaceBase
{
private:
    css::uno::Reference<css::frame::XController> mxController;
    VclPtr<DevelopmentToolDockingWindow> mpDockingWindow;

public:
    SelectionChangeHandler(const css::uno::Reference<css::frame::XController>& rxController,
                           DevelopmentToolDockingWindow* pDockingWindow)
        : SelectionChangeHandlerInterfaceBase(m_aMutex)
        , mxController(rxController)
        , mpDockingWindow(pDockingWindow)
    {
    }

    ~SelectionChangeHandler() { mpDockingWindow.disposeAndClear(); }

    virtual void SAL_CALL selectionChanged(const css::lang::EventObject& /*rEvent*/) override
    {
        uno::Reference<view::XSelectionSupplier> xSupplier(mxController, uno::UNO_QUERY);
        if (xSupplier.is())
        {
            uno::Any aAny = xSupplier->getSelection();
            auto aRef = aAny.get<uno::Reference<uno::XInterface>>();
            mpDockingWindow->introspect(aRef);
        }
    }
    virtual void SAL_CALL disposing(const css::lang::EventObject& /*rEvent*/) override {}
    virtual void SAL_CALL disposing() override {}

private:
    SelectionChangeHandler(const SelectionChangeHandler&) = delete;
    SelectionChangeHandler& operator=(const SelectionChangeHandler&) = delete;
};

} // end anonymous namespace

DevelopmentToolDockingWindow::DevelopmentToolDockingWindow(SfxBindings* pInputBindings,
                                                           SfxChildWindow* pChildWindow,
                                                           vcl::Window* pParent)
    : SfxDockingWindow(pInputBindings, pChildWindow, pParent, "DevelopmentTool",
                       "svx/ui/developmenttool.ui")
    , mpClassNameLabel(m_xBuilder->weld_label("class_name_value_id"))
    , mpClassListBox(m_xBuilder->weld_tree_view("class_listbox_id"))
    , mpLeftSideTreeView(m_xBuilder->weld_tree_view("leftside_treeview_id"))
    , maDocumentModelTreeHandler(
          mpLeftSideTreeView,
          pInputBindings->GetDispatcher()->GetFrame()->GetObjectShell()->GetBaseModel())
{
    mpLeftSideTreeView->connect_changed(LINK(this, DevelopmentToolDockingWindow, LeftSideSelected));

    auto* pViewFrame = pInputBindings->GetDispatcher()->GetFrame();

    uno::Reference<frame::XController> xController = pViewFrame->GetFrame().GetController();

    mxRoot = pInputBindings->GetDispatcher()->GetFrame()->GetObjectShell()->GetBaseModel();

    introspect(mxRoot);
    maDocumentModelTreeHandler.inspectDocument();

    uno::Reference<view::XSelectionSupplier> xSupplier(xController, uno::UNO_QUERY);
    if (xSupplier.is())
    {
        uno::Reference<view::XSelectionChangeListener> xChangeListener(
            new SelectionChangeHandler(xController, this));
        xSupplier->addSelectionChangeListener(xChangeListener);
    }
}

IMPL_LINK(DevelopmentToolDockingWindow, LeftSideSelected, weld::TreeView&, rView, void)
{
    OUString sID = rView.get_selected_id();
    auto xObject = DocumentModelTreeHandler::getObjectByID(sID);
    if (xObject.is())
        introspect(xObject);
}

DevelopmentToolDockingWindow::~DevelopmentToolDockingWindow() { disposeOnce(); }

void DevelopmentToolDockingWindow::dispose()
{
    mpClassNameLabel.reset();
    mpClassListBox.reset();
    maDocumentModelTreeHandler.dispose();
    mpLeftSideTreeView.reset();

    SfxDockingWindow::dispose();
}

void DevelopmentToolDockingWindow::ToggleFloatingMode()
{
    SfxDockingWindow::ToggleFloatingMode();

    if (GetFloatingWindow())
        GetFloatingWindow()->SetMinOutputSizePixel(Size(300, 300));

    Invalidate();
}

void DevelopmentToolDockingWindow::introspect(uno::Reference<uno::XInterface> const& xInterface)
{
    if (!xInterface.is())
        return;

    uno::Reference<uno::XComponentContext> xContext = comphelper::getProcessComponentContext();
    if (!xContext.is())
        return;

    auto xServiceInfo = uno::Reference<lang::XServiceInfo>(xInterface, uno::UNO_QUERY);
    OUString aImplementationName = xServiceInfo->getImplementationName();

    mpClassNameLabel->set_label(aImplementationName);

    mpClassListBox->freeze();
    mpClassListBox->clear();

    std::unique_ptr<weld::TreeIter> pParent = mpClassListBox->make_iterator();
    OUString aServicesString("Services");
    mpClassListBox->insert(nullptr, -1, &aServicesString, nullptr, nullptr, nullptr, false,
                           pParent.get());
    mpClassListBox->set_text_emphasis(*pParent, true, 0);

    std::unique_ptr<weld::TreeIter> pResult = mpClassListBox->make_iterator();
    const uno::Sequence<OUString> aServiceNames(xServiceInfo->getSupportedServiceNames());
    for (auto const& aServiceName : aServiceNames)
    {
        mpClassListBox->insert(pParent.get(), -1, &aServiceName, nullptr, nullptr, nullptr, false,
                               pResult.get());
    }

    uno::Reference<beans::XIntrospection> xIntrospection;
    xIntrospection = beans::theIntrospection::get(xContext);

    uno::Reference<beans::XIntrospectionAccess> xIntrospectionAccess;
    xIntrospectionAccess = xIntrospection->inspect(uno::makeAny(xInterface));

    OUString aPropertiesString("Properties");
    mpClassListBox->insert(nullptr, -1, &aPropertiesString, nullptr, nullptr, nullptr, false,
                           pParent.get());
    mpClassListBox->set_text_emphasis(*pParent, true, 0);

    const auto xProperties = xIntrospectionAccess->getProperties(
        beans::PropertyConcept::ALL - beans::PropertyConcept::DANGEROUS);
    for (auto const& xProperty : xProperties)
    {
        mpClassListBox->insert(pParent.get(), -1, &xProperty.Name, nullptr, nullptr, nullptr, false,
                               pResult.get());
    }

    OUString aMethodsString("Methods");
    mpClassListBox->insert(nullptr, -1, &aMethodsString, nullptr, nullptr, nullptr, false,
                           pParent.get());
    mpClassListBox->set_text_emphasis(*pParent, true, 0);

    const auto xMethods = xIntrospectionAccess->getMethods(beans::MethodConcept::ALL);
    for (auto const& xMethod : xMethods)
    {
        OUString aMethodName = xMethod->getName();
        mpClassListBox->insert(pParent.get(), -1, &aMethodName, nullptr, nullptr, nullptr, false,
                               pResult.get());
    }

    mpClassListBox->thaw();
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
