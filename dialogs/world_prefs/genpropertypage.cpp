#include "stdafx.h"
#include "..\..\MUSHclient.h"
#include "..\..\mainfrm.h"
#include "..\..\doc.h"
#include "genpropertypage.h"
#include "..\EditMultiLine.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGenPropertyPage property page
IMPLEMENT_DYNAMIC(CGenPropertyPage, CPropertyPage)

CGenPropertyPage::CGenPropertyPage(const UINT nID) : 
                                   CPropertyPage(nID)
{
	//{{AFX_DATA_INIT(CGenPropertyPage)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

  m_last_col = 0;
  m_reverse = FALSE;

  m_iColWidth           = NULL;
  m_iColJust            = NULL;
  m_strColumnHeadings   = NULL;
  m_iColumnCount        = 0;
  m_bWantTreeControl    = false;

}

CGenPropertyPage::~CGenPropertyPage()
{

  delete [] m_iColWidth;            
  delete [] m_strColumnHeadings;    
  delete [] m_iColJust;            

}

void CGenPropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGenPropertyPage)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CGenPropertyPage, CPropertyPage)
	//{{AFX_MSG_MAP(CGenPropertyPage)
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
  ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGenPropertyPage message handlers


void CGenPropertyPage::SetUpPage (CString strObjectType,
                                  CObjectMap * ObjectMap,
                                  CListCtrl * ctlList,
                                  CStatic * ctlSummary,
                                  t_CompareObjects CompareObjects,
                                  CFindInfo * pFindInfo,
                                  const unsigned long iMask,
                                  const bool bWantTreeControl)
  {
  m_strObjectType = strObjectType;
  m_ObjectMap = ObjectMap;
  m_ctlList = ctlList;
  m_ctlSummary = ctlSummary;
  m_CompareObjects = CompareObjects;
  m_pObjectFindInfo = pFindInfo;
  m_nUpdateNumber = App.GetUniqueNumber ();
  m_iMask = iMask;
  m_bWantTreeControl = bWantTreeControl;

  }


int CGenPropertyPage::GetSelectedItemCount () const
  {

  int iCount = 0;

  if (m_bWantTreeControl)
    {

    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {

      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            {
            if (m_cTreeCtrl.GetItemState (hItem, TVIS_SELECTED) & TVIS_SELECTED)
              iCount++;
            }   // end for each item

      }  // end for each group


    return iCount;
    } // end of tree control

  return m_ctlList->GetSelectedCount ();

  } // end of CGenPropertyPage::GetSelectedItemCount 

int CGenPropertyPage::GetItemCount () const
  {

  int iCount = 0;

  if (m_bWantTreeControl)
    {

    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {

      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            iCount++;

      }  // end for each group


    return iCount;
    } // end of tree control

  return m_ctlList->GetItemCount ();

  } // end of CGenPropertyPage::GetSelectedItemCount 


/////////////////////////////////////////////////////////////////////////////
// OnAddItem

void CGenPropertyPage::OnAddItem(CDialog & dlg) 
{

  // load dialog with default values
  InitDialog (&dlg);

  // display dialog, exit if cancelled
  if (dlg.DoModal () != IDOK)
    return;

  // create the object's lookup name (label converted to lower case, or generated)
 CObject * pItem;
 CString strObjectName;

  strObjectName = GetObjectName (&dlg);
  if (strObjectName.IsEmpty ())
    strObjectName.Format ("*%s%s",       // e.g. *trigger100
                            (LPCTSTR) m_strObjectType,
                            (LPCTSTR) App.GetUniqueString ());
  else
    strObjectName.MakeLower();

  // if already there (presumably not possible with un-named objects),
  // then abort the add
  if (m_ObjectMap->Lookup (strObjectName, pItem))
    {
    CString strMsg;

    strMsg = TFormat ("The %s named \"%s\" is already in the %s list",
                  (LPCTSTR) m_strObjectType,
                  (LPCTSTR) GetObjectName (&dlg),
                  (LPCTSTR) m_strObjectType);
    ::UMessageBox (strMsg);
    return;
    }

  // add new object to map
  m_ObjectMap->SetAt (strObjectName, pItem = MakeNewObject ());

  // unload from dialog into object's properties
  UnloadDialog (&dlg, pItem);

  // They can no longer cancel the property sheet, the document has changed
  CancelToClose ();
  if (!CheckIfTemporary (pItem))
    m_doc->SetModifiedFlag (TRUE);

  // create a CString for lookup purposes
  CString * pstrObjectName = new CString (strObjectName);

  // add this item to the list view
  if (m_bWantTreeControl)
    add_tree_item (pItem, pstrObjectName);
  else
    add_list_item (pItem, pstrObjectName, 0, TRUE);

  SetInternalName (pItem, strObjectName);  // set name so we can delete one-shot items

  // resort the list

  SortItems ();

  // redraw the list
  if (GetFilterFlag ())
    LoadList ();       // full reload because it may have changed filter requirements

// get dispatch id from the script and put it into the item

  if (m_doc->m_ScriptEngine)
    {
    CString strMessage;
    SetDispatchID (pItem, m_doc->GetProcedureDispid (GetScriptName (pItem),
                                                     m_strObjectType, 
                                                     GetLabel (pItem),
                                                     strMessage));
    if (!strMessage.IsEmpty ())
      ::UMessageBox (strMessage, MB_ICONINFORMATION);
    }
  
}    // end of CGenPropertyPage::OnAddItem

/////////////////////////////////////////////////////////////////////////////
// CheckParentHasChildren

void CGenPropertyPage::CheckParentHasChildren (HTREEITEM hdlParent)
  {
  if (m_cTreeCtrl.GetChildItem (hdlParent) == NULL)
    {
    // get (old) group name
    CString strParent = m_cTreeCtrl.GetItemText (hdlParent);

    // delete group from tree view
    m_cTreeCtrl.DeleteItem (hdlParent);

    // find parent in map
    map<CString, HTREEITEM>::iterator it = m_GroupsMap.find (strParent);
    // delete from map of parents (groups) as well
    if (it != m_GroupsMap.end ())
      m_GroupsMap.erase (it);

    }   // end of no children

  }  // end of CGenPropertyPage::CheckParentHasChildren


/////////////////////////////////////////////////////////////////////////////
// ChangeOneItem

bool CGenPropertyPage::ChangeOneItem (CDialog & dlg, CString * pstrObjectName, const int nItem, HTREEITEM hdlItem)
  {
CString strMsg;

  ASSERT (pstrObjectName != NULL);

  CObject * pItem;

  // check object is still there (it might have gone while we looked at the list box)
  if (!m_ObjectMap->Lookup (*pstrObjectName, pItem))
    {
    if (hdlItem)
      {
      HTREEITEM hdlParent = m_cTreeCtrl.GetParentItem (hdlItem);
      m_cTreeCtrl.DeleteItem (hdlItem);
      CheckParentHasChildren (hdlParent);
      }
    else
      {
      m_ctlList->DeleteItem (nItem);    // it's gone, so delete it from the list view
      m_ctlList->RedrawItems (0, m_ctlList->GetItemCount () - 1);    // redraw the list
      }

    // in the case of one-shot timers, unnamed items might be removed from the list
    if (pstrObjectName->Left (1) == "*")
      strMsg = TFormat ("The %s you selected is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) m_strObjectType);
    else
      strMsg = TFormat ("The %s named \"%s\" is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) *pstrObjectName,
                    (LPCTSTR) m_strObjectType);

    ::UMessageBox (strMsg);

    delete pstrObjectName;                 // and get rid of its name string
    return false;
    }

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );


  // load dialog with values from the found item
  LoadDialog (&dlg, pItem);

  // put up the dialog, give up if they cancel
  if (dlg.DoModal () != IDOK)
    return false;

  // lookup this object, to make sure it still exists
  if (!m_ObjectMap->Lookup (*pstrObjectName, pItem))
    {
    if (hdlItem)
      {
      HTREEITEM hdlParent = m_cTreeCtrl.GetParentItem (hdlItem);
      m_cTreeCtrl.DeleteItem (hdlItem);
      CheckParentHasChildren (hdlParent);
      }
    else
      {
      m_ctlList->DeleteItem (nItem);    // it's gone, so delete it from the list view
      m_ctlList->RedrawItems (0, m_ctlList->GetItemCount () - 1);    // redraw the list
      }
    
    // in the case of one-shot timers, unnamed items might be removed from the list
    if (pstrObjectName->Left (1) == "*")
      strMsg = TFormat ("The %s you selected is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) m_strObjectType);
    else
      strMsg = TFormat ("The %s named \"%s\" is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) *pstrObjectName,
                    (LPCTSTR) m_strObjectType);

    delete pstrObjectName;                 // and get rid of its name string
    ::UMessageBox (strMsg);
    return false;
    }

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );

  if (CheckIfIncluded (pItem))
    {
    strMsg = TFormat ("The %s named \"%s\" has been included from an include file. You cannot modify it here.",
                  (LPCTSTR) m_strObjectType,
                  (LPCTSTR) *pstrObjectName);
    ::UMessageBox (strMsg);
    return true; // can't modify included items
    }

  // check object still has the same modification number
  // (it might have been modified while we looked at the list box)
  if (GetModificationNumber (pItem) != m_nUpdateNumber)
    {
    strMsg = TFormat ("The %s named \"%s\" has already been modified by a script subroutine",
                  (LPCTSTR) m_strObjectType,
                  (LPCTSTR) *pstrObjectName);
    ::UMessageBox (strMsg);
    return false;
    }

  // check for name change 
  CString strObjectName = GetObjectName (&dlg);
  if (strObjectName.IsEmpty ())
    strObjectName.Format ("*%s%s", 
                          (LPCTSTR) m_strObjectType,
                          (LPCTSTR) App.GetUniqueString ());
  else
    strObjectName.MakeLower ();
      
  if (strObjectName != *pstrObjectName)     // has name changed?
    {
    // here if label has changed
    CObject * new_pItem;
    if (m_ObjectMap->Lookup (strObjectName, new_pItem))
      {
      strMsg = TFormat ("The %s named \"%s\" already exists in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) strObjectName,
                    (LPCTSTR) m_strObjectType);
      ::UMessageBox (strMsg);
      return false;
      }

    // remove old entry and re-add under new name
    m_ObjectMap->RemoveKey (*pstrObjectName);     // remove old entry
    m_ObjectMap->SetAt (strObjectName, pItem);   // insert under new name

    // delete old name in the list
    delete pstrObjectName;

    // create a new CString for lookup purposes
    pstrObjectName = new CString (strObjectName);

    // record item's new name as the list object data
    if (hdlItem)
      m_cTreeCtrl.SetItemData (hdlItem, (DWORD) pstrObjectName);
    else
      m_ctlList->SetItemData (nItem, (DWORD) pstrObjectName);
    }   // end of label changing

  // see if the user changed anything, anyway
  if (CheckIfChanged (&dlg, pItem))
    {

    // unload from dialog into object's properties
    UnloadDialog (&dlg, pItem);

    // They can no longer cancel the property sheet, the document has changed
    CancelToClose ();
    if (!CheckIfTemporary (pItem))
      m_doc->SetModifiedFlag (TRUE);


    if (m_bWantTreeControl)
      {
      // group may have changed, delete and re-add
      HTREEITEM hdlParent = m_cTreeCtrl.GetParentItem (hdlItem);
      m_cTreeCtrl.DeleteItem (hdlItem);
      HTREEITEM hItem = add_tree_item (pItem, pstrObjectName); 

      // if deleting item deletes only one in group, remove group as well
      CheckParentHasChildren (hdlParent);

      SortItems ();

      // get its new parent
      hdlParent = m_cTreeCtrl.GetParentItem (hItem);
      m_cTreeCtrl.SetItemState (hdlParent, TVIS_EXPANDED, TVIS_EXPANDED); // expand group (parent)
      // select the new item
      m_cTreeCtrl.SelectItem (hItem);
      m_cTreeCtrl.EnsureVisible (hItem);  // may have changed groups
      }
    else
      {
      // re-setup list with amended details
      int nNewItem = add_list_item (pItem, pstrObjectName, nItem, FALSE);  // replace
      m_ctlList->RedrawItems (nNewItem, nNewItem);
      }

    }   // end of item changing

// Get dispatch id from the script and put it into the item.
// We do this even if nothing has changed, so that we can force re-evaluation
// of the dispatch ID, by just getting the item and pressing OK.

  if (m_doc->m_ScriptEngine)
    {
    CString strMessage;
    SetDispatchID (pItem, m_doc->GetProcedureDispid (GetScriptName (pItem),
                                                     m_strObjectType, 
                                                     GetLabel (pItem),
                                                     strMessage));
    if (!strMessage.IsEmpty ())
      ::UMessageBox (strMessage, MB_ICONINFORMATION);
    }

  return false;
  } // end of CGenPropertyPage::ChangeOneItem

/////////////////////////////////////////////////////////////////////////////
// OnChangeItem

void CGenPropertyPage::OnChangeItem(CDialog & dlg) 
{

  if (m_bWantTreeControl)
    {

    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {

      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            {
            UINT iState = m_cTreeCtrl.GetItemState (hItem, TVIS_SELECTED);

            if (iState & TVIS_SELECTED)
              {
              if (ChangeOneItem (dlg, (CString *) m_cTreeCtrl.GetItemData (hItem), 0, hItem))
                break;
              }  // end if selected

            }   // end for each item

      }  // end for each group


    } // end of tree control
  else
    {
    // iterate through list in case we implement multiple selection one day
    for (int nItem = -1;
          (nItem = m_ctlList->GetNextItem(nItem, LVNI_SELECTED)) != -1;)
      {

      if (ChangeOneItem (dlg, (CString *) m_ctlList->GetItemData (nItem), nItem, NULL))
        break;

      }   // end of dealing with each selected item

  }   // end of list control

  // redraw the list
  if (GetFilterFlag ())
    LoadList ();       // full reload because it may have changed filter requirements

  // resort the list

  SortItems ();

}   // end of CGenPropertyPage::OnChangeItem


/////////////////////////////////////////////////////////////////////////////
// DeleteOneItem

bool CGenPropertyPage::DeleteOneItem(CString * pstrObjectName, int & iIncluded, int & iExecuting)
  {

  ASSERT (pstrObjectName != NULL);

  CObject * pItem;

  // see if in the map
  if (!m_ObjectMap->Lookup (*pstrObjectName, pItem))
    return true;   // already deleted!

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );

  if (CheckIfIncluded (pItem))
    {
    iIncluded++;    // don't do message here in case hundreds of them
    return true;
    }

  if (CheckIfExecuting (pItem))
    {
    iExecuting++;    // don't do message here in case hundreds of them
    return true;
    }

  // They can no longer cancel the property sheet, the document has changed
  CancelToClose ();
  if (!CheckIfTemporary (pItem))
    m_doc->SetModifiedFlag (TRUE);

  // delete from the map
  m_ObjectMap->RemoveKey (*pstrObjectName);

  // delete the item itself
  delete pItem;

  // delete its item string
  delete pstrObjectName;

  return false; // OK return
  }

/////////////////////////////////////////////////////////////////////////////
// OnDeleteItem

void CGenPropertyPage::OnDeleteItem() 
{

int iCount =  GetSelectedItemCount ();
int nItem,
    i,
    iIncluded = 0,
    iExecuting = 0;


  if (iCount == 0)
    return;

  if (App.m_bTriggerRemoveCheck)
    {
    // mucking around to make it plural properly
    CString sName =  m_strObjectType;
    if (iCount > 1)
      if (sName == "alias")
        sName += "es";
      else
        sName += "s";

    if (::UMessageBox (TFormat ("Delete %i %s - are you sure?",
        iCount, sName), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2)
        != IDYES )
        return;
    } // end of wanting to confirm


  // tree control ................
  if (m_bWantTreeControl)
    {
    // go through entire tree control
    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {

      // find selected items
      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            {
            UINT iState = m_cTreeCtrl.GetItemState (hItem, TVIS_SELECTED);

            if (iState & TVIS_SELECTED)
              {
              CString * pstrObjectName = (CString *) m_cTreeCtrl.GetItemData (hItem);

              if (!DeleteOneItem (pstrObjectName, iIncluded, iExecuting))
                {
                HTREEITEM hdlParent = m_cTreeCtrl.GetParentItem (hItem);
                m_cTreeCtrl.DeleteItem (hItem);
                CheckParentHasChildren (hdlParent);
                }

              }  // end if selected

            }   // end for each item

      }  // end for each group

    }   // end of tree control
  else
    // list control ................
    {
    CUIntArray arySelected;
    arySelected.SetSize (iCount);

      // first, remember selected items
    for (nItem = -1, i = 0;
          (nItem = m_ctlList->GetNextItem(nItem, LVNI_SELECTED)) != -1;)
           arySelected [i++] = nItem;

    // we do it this way because deleting items buggers up the position in the array
    for (i = iCount - 1; i >= 0; i--)
      {
      nItem = arySelected [i];

      // get the lower-case name of this item's object
      CString * pstrObjectName = (CString *) m_ctlList->GetItemData (nItem);

      if (!DeleteOneItem (pstrObjectName, iIncluded, iExecuting))
        m_ctlList->DeleteItem (nItem);

      }   // end of dealing with each selected item

    }  // end of list control

  if (iIncluded)
    {
    CString strMsg;
    strMsg = TFormat ("%i item%s %s included from an include file. You cannot delete %s here.",
                   PLURAL (iIncluded),
                   iIncluded == 1 ? "was" : "were",
                   iIncluded == 1 ? "it" : "them");
      ::UMessageBox (strMsg);
    }

  if (iExecuting)
    {
    CString strMsg;
    strMsg = TFormat ("%i item%s %s currently executing a script. You cannot delete %s now.",
                   PLURAL (iExecuting),
                   iExecuting == 1 ? "is" : "are",
                   iExecuting == 1 ? "it" : "them");
      ::UMessageBox (strMsg);
    }

}   // end of CGenPropertyPage::OnDeleteItem


/////////////////////////////////////////////////////////////////////////////
// OnColumnclickItemList

void CGenPropertyPage::OnColumnclickItemList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

int col = pNMListView->iSubItem;

  if (col == m_last_col)
    m_reverse = !m_reverse;
  else
    m_reverse = FALSE;

  m_last_col = col;

  t_gen_sort_param sort_param (m_ObjectMap, m_last_col, m_reverse, m_CompareObjects);

  m_ctlList->SortItems (CompareFunc, (LPARAM) &sort_param); 
	
	*pResult = 0;
}    // end of CGenPropertyPage::OnColumnclickItemList


/////////////////////////////////////////////////////////////////////////////
// add_list_item

int CGenPropertyPage::add_list_item (CObject * pItem, 
               const CString  * pstrObjectName,
               const int nItem,
               const BOOL bInsert)
  {

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );
  ASSERT (pstrObjectName != NULL);

  // add to control
  int nNewItem = AddItem (pItem, nItem, bInsert);

  // record item's name as the list object data
  m_ctlList->SetItemData(nNewItem, (DWORD) pstrObjectName);

  // stamp with a number so we know if it was updated without our knowledge
  SetModificationNumber (pItem, m_nUpdateNumber);

  return nNewItem;

  } // end of CGenPropertyPage::add_list_item


/////////////////////////////////////////////////////////////////////////////
// add_tree_item

HTREEITEM CGenPropertyPage::add_tree_item (CObject * pItem, 
                                           const CString  * pstrObjectName)
  {
  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );
  ASSERT (pstrObjectName != NULL);

  // get group name
  CString strGroup = GetGroup (pItem);
  if (strGroup.IsEmpty ())
    strGroup = Translate ("(ungrouped)");

  // see if group exists already
  map<CString, HTREEITEM>::const_iterator it;

  it = m_GroupsMap.find (strGroup);

  HTREEITEM hParent;

  // if it exists, find the group tree item (the parent)
  // if not, create it
  if (it == m_GroupsMap.end ())
    {
    hParent = m_cTreeCtrl.InsertItem (strGroup,TVI_ROOT);
    m_GroupsMap [strGroup] = hParent;
    }
  else
    hParent = it->second;

  // get the description for the tree control
  CString strDescription = GetDescription  (pItem);

  // truncate if ridiculously long
  if (strDescription.GetLength () > 100)
    strDescription = strDescription.Left (100) + " ...";

  CString strLabel = GetLabel (pItem);

  // add the label if it exists
  if (!strLabel.IsEmpty ())
    {
    strDescription = strDescription + " [";
    strDescription = strDescription + strLabel;
    strDescription = strDescription + "]";
    }

  // insert it
  HTREEITEM hNewItem = m_cTreeCtrl.InsertItem (strDescription, hParent);

  // record item's name as the list object data
  m_cTreeCtrl.SetItemData (hNewItem, (DWORD) pstrObjectName);

  // stamp with a number so we know if it was updated without our knowledge
  SetModificationNumber (pItem, m_nUpdateNumber);

  return hNewItem;
  }  // end of CGenPropertyPage::add_tree_item

/////////////////////////////////////////////////////////////////////////////
// CompareFunc

int CALLBACK CGenPropertyPage::CompareFunc ( LPARAM lParam1, 
                                             LPARAM lParam2,
                                             LPARAM lParamSort)
  { 

 ASSERT (lParamSort != NULL);

t_gen_sort_param * psort_param = (t_gen_sort_param *) lParamSort;

CString * pName1 = (CString *) lParam1;
CString * pName2 = (CString *) lParam2;

CObject * item1,
        * item2;

 ASSERT (pName1 != NULL);
 ASSERT (pName2 != NULL);

 if (psort_param->pObjectMap->Lookup(*pName1, item1) == 0)
   return 0;
 if (psort_param->pObjectMap->Lookup(*pName2, item2) == 0)
   return 0;

 ASSERT_VALID (item1);
 ASSERT( item1->IsKindOf( RUNTIME_CLASS( CObject ) ) );
 ASSERT_VALID (item2);
 ASSERT( item2->IsKindOf( RUNTIME_CLASS( CObject ) ) );

t_CompareObjects pCompare = psort_param->CompareObjects;

int iResult = (*pCompare) (psort_param->sortkey, item1, item2);

// if reverse sort wanted, reverse sense of result

  if (psort_param->reverse)
    iResult *= -1;

  return iResult;

  } // end of CGenPropertyPage::CompareFunc


/////////////////////////////////////////////////////////////////////////////
// SortItems

void CGenPropertyPage::SortItems (void)                 
  {

  t_gen_sort_param sort_param (m_ObjectMap, m_last_col, m_reverse, m_CompareObjects);

  if (m_bWantTreeControl)
    {
    // sort root level into groups
    m_cTreeCtrl.SortChildren (TVI_ROOT);

    TVSORTCB cbinfo;
    cbinfo.lpfnCompare = CompareFunc;
    cbinfo.lParam = (LPARAM) &sort_param;

    // now sort each child
    HTREEITEM hdlItem = m_cTreeCtrl.GetNextItem (NULL, TVGN_ROOT);

    while (hdlItem)
      {
      cbinfo.hParent = hdlItem;
      m_cTreeCtrl.SortChildrenCB (&cbinfo );
      hdlItem = m_cTreeCtrl.GetNextItem (hdlItem, TVGN_NEXT);
      }

    }
  else
    m_ctlList->SortItems (CompareFunc, (LPARAM) &sort_param); 


  } // end of  CGenPropertyPage::SortItems

/////////////////////////////////////////////////////////////////////////////
// LoadList

void CGenPropertyPage::LoadList (void)
  {
  long iCount = 0;
  long iNotShown = 0;

  // for filtering

  CScriptEngine * m_ScriptEngine = NULL;    // for the filtering checks
  bool bFiltering = GetFilterFlag ();

  
  if (bFiltering)
    {
    m_ScriptEngine = new CScriptEngine (m_doc, "Lua");

    if (m_ScriptEngine->CreateScriptEngine ())
      bFiltering = false;
    else
      {

        // compile filter script
       try
         {

          if (m_ScriptEngine->Parse (GetFilterScript (), "Script file"))
            bFiltering = false;

         }  // end of try
       catch (CException * e)
         {
          e->ReportError ();
          e->Delete ();
          bFiltering = false;
         }
      }  // not error creating engine

    }  // end of filtering wanted

  lua_State * L = NULL;

  // if filtering, get the "filter" function on the stack
  if (bFiltering)
    {
    L = m_ScriptEngine->L;   // make copy for convenience

    lua_settop(L, 0);   // clear stack, just in case

    if (!GetNestedFunction (L, "filter", true))
        bFiltering = false;
    }

  // we delete from both the list and the tree in case we are switching
  // from list view to tree view or vice-versa

  // remove all old list items (we used the item data to key to the item)

  for (int nItem = 0; nItem < m_ctlList->GetItemCount (); nItem++)
    delete (CString *) m_ctlList->GetItemData (nItem);

   m_ctlList->DeleteAllItems ();
  

  // and now all old tree items
  for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
       hGroup;
       hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
    {

    for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
        hItem;
        hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
      delete (CString *) m_cTreeCtrl.GetItemData (hItem);

    }  // end for each group

   m_cTreeCtrl.DeleteAllItems ();
   // since all is deleted, we don't have any groups any more
   m_GroupsMap.erase (m_GroupsMap.begin (), m_GroupsMap.end ());

   CString strObjectName;
   CObject * pItem;

// Item data

   for (POSITION pos = m_ObjectMap->GetStartPosition (); pos; )
     {
     m_ObjectMap->GetNextAssoc (pos, strObjectName, pItem);
     bool bUse = true;  // defaults to true if no filtering

     if (bFiltering)
       {
       lua_pushvalue(L, 1);        // filter function
       lua_pushstring (L, (const char *) strObjectName);     // key of the item
       GetFilterInfo (pItem, L);   // table of related info
       if (lua_pcall (L, 2, 1, 0))   // call with 1 arg1 and 1 result
         {
         LuaError (L);
         bFiltering = false;
         }
       else
         {
         // use result if we get the exact type: boolean (true/false)
         if (lua_isboolean (L, -1))
           {
           bUse = lua_toboolean (L, -1);
           if (!bUse)
             iNotShown++;  // this one not shown
           }
         lua_pop (L, 1);  // pop result
         }

       }  // end of filtering wanted

     if (bUse)  // add to list if passed filter
       {
       CString * pstrObjectName = new CString (strObjectName);
       if (m_bWantTreeControl)
         add_tree_item (pItem, pstrObjectName); 
       else
         add_list_item (pItem, pstrObjectName, 0, TRUE); 
       iCount++;
       }
     }

  SortItems ();

// set the 1st item to be selected - we do this here because sorting the
// list means our first item is not necessarily the 1st item in the list

 if (!m_ObjectMap->IsEmpty ())    // provided we have any
   m_ctlList->SetItemState (0, LVIS_FOCUSED | LVIS_SELECTED, 
                               LVIS_FOCUSED | LVIS_SELECTED);

  CString strSummary = TFormat ("%i item%s.", PLURAL (iCount));

  if (iNotShown)
    strSummary += TFormat (" (%i item%s hidden by filter)", PLURAL (iNotShown));
  m_ctlSummary->SetWindowText (strSummary);

  delete m_ScriptEngine;

  }  // end of CGenPropertyPage::LoadList


/////////////////////////////////////////////////////////////////////////////
// OnKickIdle

LRESULT CGenPropertyPage::OnKickIdle(WPARAM, LPARAM)
  {
  UpdateDialogControls (AfxGetApp()->m_pMainWnd, false);
  return 0;
  } // end of CGenPropertyPage::OnKickIdle

/////////////////////////////////////////////////////////////////////////////
// OnUpdateNeedSelection

void CGenPropertyPage::OnUpdateNeedSelection(CCmdUI* pCmdUI)
  {
    pCmdUI->Enable (GetSelectedItemCount () != 0);
  } // end of CGenPropertyPage::OnUpdateNeedSelection

/////////////////////////////////////////////////////////////////////////////
// OnUpdateNeedEntries

void CGenPropertyPage::OnUpdateNeedEntries(CCmdUI* pCmdUI)
  {
  pCmdUI->Enable (GetItemCount () > 0);
  } // end of CGenPropertyPage::OnUpdateNeedEntries


/////////////////////////////////////////////////////////////////////////////
// get_tree_item

HTREEITEM CGenPropertyPage::get_tree_item (CTreeCtrl * pTree, const int n)
  {
  HTREEITEM hItem = NULL;
  int iCount = 0;
  bool bFound = false;

  for (HTREEITEM hGroup = pTree->GetRootItem ();
       hGroup && !bFound;
       hGroup = pTree->GetNextSiblingItem (hGroup))
    {
    for (hItem = pTree->GetChildItem (hGroup);
        hItem && !bFound;
        hItem = pTree->GetNextSiblingItem (hItem))
          {
          if (iCount == n)
            {
            bFound = true;
            break;
            }
          iCount++;
          }
    }  // end for each group

  if (bFound)
    return hItem;

  return NULL;

  }  // end of CGenPropertyPage::get_tree_item

/////////////////////////////////////////////////////////////////////////////
// DoFind

void CGenPropertyPage::DoFind (bool bAgain)
  {

m_pObjectFindInfo->m_bAgain = bAgain;
m_pObjectFindInfo->m_nTotalLines = GetItemCount ();

bool found;

  if (m_bWantTreeControl)
    {

    found = FindRoutine (this,       // passed back to callback routines
                        *m_pObjectFindInfo,   // finding structure
                        InitiateSearch,        // how to re-initiate a find
                        GetNextTreeLine);          // get the next line
    // unselect everything first
    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {
      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            m_cTreeCtrl.SetItemState (hItem, 0, TVIS_SELECTED);
      }  // end for each group

    if (found)
      {

      HTREEITEM hItem = get_tree_item (&m_cTreeCtrl, m_pObjectFindInfo->m_nCurrentLine);

      if (!hItem)
        return;

      // select the new item
      m_cTreeCtrl.SelectItem (hItem);
      m_cTreeCtrl.EnsureVisible (hItem);    

      }

    } // end of tree control
  else
    {
    found = FindRoutine (this,       // passed back to callback routines
                          *m_pObjectFindInfo,   // finding structure
                          InitiateSearch,        // how to re-initiate a find
                          GetNextListLine);          // get the next line
    // unselect everything first
    for (int i = 0; i < m_ctlList->GetItemCount (); i++)
      m_ctlList->SetItemState (i, 0, LVIS_FOCUSED | LVIS_SELECTED);

    if (found)
      {
       m_ctlList->SetItemState (m_pObjectFindInfo->m_nCurrentLine, 
                                       LVIS_FOCUSED | LVIS_SELECTED, 
                                       LVIS_FOCUSED | LVIS_SELECTED);
       m_ctlList->EnsureVisible (m_pObjectFindInfo->m_nCurrentLine, false);
       m_ctlList->RedrawItems (m_pObjectFindInfo->m_nCurrentLine, 
                              m_pObjectFindInfo->m_nCurrentLine);

      }

    }



  } // end of CGenPropertyPage::DoFind


/////////////////////////////////////////////////////////////////////////////
// InitiateSearch

void CGenPropertyPage::InitiateSearch (const CObject * pObject,
                                      CFindInfo & FindInfo)
  {
  if (FindInfo.m_bAgain)
    FindInfo.m_pFindPosition = (POSITION) FindInfo.m_nCurrentLine;
  else
    if (FindInfo.m_bForwards)
      FindInfo.m_pFindPosition = 0;
    else
      FindInfo.m_pFindPosition = (POSITION) FindInfo.m_nTotalLines - 1;

  } // end of CGenPropertyPage::InitiateSearch

/////////////////////////////////////////////////////////////////////////////
// GetNextListLine

bool CGenPropertyPage::GetNextListLine (const CObject * pObject,
                                       CFindInfo & FindInfo, 
                                       CString & strLine)
  {
CGenPropertyPage * pPropPage = (CGenPropertyPage *) pObject;

CListCtrl* pList = pPropPage->m_ctlList;

  if ((long) FindInfo.m_pFindPosition < 0 ||
      (long) FindInfo.m_pFindPosition >= FindInfo.m_nTotalLines)
    return true;


// append each column contents to the find string

  strLine.Empty ();
  for (int i = 0; i < FindInfo.m_iControlColumns; i++)
    {
    if (i != 0)
      strLine += '\t';
    strLine += pList->GetItemText((long) FindInfo.m_pFindPosition, i);
    }
  
  if (FindInfo.m_bForwards)
    FindInfo.m_pFindPosition++;
  else
    FindInfo.m_pFindPosition--;

  return false;
  } // end of CGenPropertyPage::GetNextListLine

/////////////////////////////////////////////////////////////////////////////
// GetNextTreeLine

bool CGenPropertyPage::GetNextTreeLine (const CObject * pObject,
                                       CFindInfo & FindInfo, 
                                       CString & strLine)
  {
CGenPropertyPage * pPropPage = (CGenPropertyPage *) pObject;

CTreeCtrl* pTree = &pPropPage->m_cTreeCtrl;

  if ((long) FindInfo.m_pFindPosition < 0 ||
      (long) FindInfo.m_pFindPosition >= FindInfo.m_nTotalLines)
    return true;


  HTREEITEM hItem = get_tree_item (pTree, (int) FindInfo.m_pFindPosition);

  if (!hItem)
    return true;


  CString * pstrObjectName = (CString *) pTree->GetItemData (hItem);

  
  ASSERT (pstrObjectName != NULL);

  CObject * pItem;

  if (!pPropPage->m_ObjectMap->Lookup (*pstrObjectName, pItem))
    return true;

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );

  strLine = pPropPage->GetFindText (pItem);

  if (FindInfo.m_bForwards)
    FindInfo.m_pFindPosition++;
  else
    FindInfo.m_pFindPosition--;

  return false;
  } // end of CGenPropertyPage::GetNextTreeLine


/////////////////////////////////////////////////////////////////////////////
// OnInitDialog

BOOL CGenPropertyPage::OnInitDialog() 
{
	CPropertyPage::OnInitDialog();

  // Tree View stuff

  WINDOWPLACEMENT wndpl;        // where original control is
  m_ctlList->GetWindowPlacement (&wndpl);
  
  // add in the tree control
	m_cTreeCtrl.CWnd::CreateEx(WS_EX_CLIENTEDGE,
							 WC_TREEVIEW, 
							 NULL, 
							 WS_CHILD|WS_VISIBLE|WS_TABSTOP|TVS_SHOWSELALWAYS|
							 TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS, 
               wndpl.rcNormalPosition.left,
               wndpl.rcNormalPosition.top,
               wndpl.rcNormalPosition.right - wndpl.rcNormalPosition.left,
               wndpl.rcNormalPosition.bottom - wndpl.rcNormalPosition.top,
							 GetSafeHwnd(), 
							 (HMENU)ID_TREEVIEW);

  
  // now hide one of the controls
  if (m_bWantTreeControl)
    m_ctlList->ShowWindow (SW_HIDE);
  else
    m_cTreeCtrl.ShowWindow (SW_HIDE);

  // end tree view stuff

  CString strDescription = m_strObjectType;
  strDescription += " list";

	int i;
  int * iColOrder = new int [m_iColumnCount];

  // default column order is initial order (ie. 0, 1, 2, 3 etc.)
  for (i = 0; i < m_iColumnCount; i++)
    iColOrder [i] = i;

  App.RestoreColumnConfiguration (strDescription, m_iColumnCount, *m_ctlList,
                                  iColOrder, m_iColWidth, m_last_col, m_reverse);

  for (i = 0; i < m_iColumnCount; i++)
    m_ctlList->InsertColumn(i, m_strColumnHeadings [i], m_iColJust [i], m_iColWidth [i]);

// recover column sequence

  m_ctlList->SendMessage (LVM_SETCOLUMNORDERARRAY, m_iColumnCount, (DWORD) iColOrder);

  LoadList ();
  
  delete [] iColOrder;

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////
// OnDestroy

void CGenPropertyPage::OnDestroy() 
{

  CString strDescription = m_strObjectType;
  strDescription += " list";

// delete the object names which we stored as the item data

  for (int nItem = 0; nItem < m_ctlList->GetItemCount (); nItem++)
    delete (CString *) m_ctlList->GetItemData (nItem);

  // and now all old tree items
  for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
       hGroup;
       hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
    {

    for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
        hItem;
        hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
      delete (CString *) m_cTreeCtrl.GetItemData (hItem);

    }  // end for each group

  App.SaveColumnConfiguration (strDescription, m_iColumnCount, *m_ctlList, 
                                m_last_col, m_reverse);

	CPropertyPage::OnDestroy();
		
}

/////////////////////////////////////////////////////////////////////////////
// OnFind

void CGenPropertyPage::OnFind() 
{
  DoFind (false);
}    // end of CGenPropertyPage::OnFind

/////////////////////////////////////////////////////////////////////////////
// OnFindNext

void CGenPropertyPage::OnFindNext() 
{
  DoFind (true);
}   // end of CGenPropertyPage::OnFindNext


/////////////////////////////////////////////////////////////////////////////
// CopyOneItem

bool CGenPropertyPage::CopyOneItem(CString * pstrObjectName, CArchive & ar)
  {
  ASSERT (pstrObjectName != NULL);

  CObject * pItem;

  // check object is still there (it might have gone while we looked at the list box)
  if (!m_ObjectMap->Lookup (*pstrObjectName, pItem))
    {
    CString strMsg;

    // in the case of one-shot timers, unnamed items might be removed from the list
    if (pstrObjectName->Left (1) == "*")
      strMsg = TFormat ("The %s you selected is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) m_strObjectType);
    else
      strMsg = TFormat ("The %s named \"%s\" is no longer in the %s list",
                    (LPCTSTR) m_strObjectType,
                    (LPCTSTR) *pstrObjectName,
                    (LPCTSTR) m_strObjectType);

    ::UMessageBox (strMsg);

    delete pstrObjectName;                 // and get rid of its name string
    return true;
    }

  ASSERT_VALID (pItem);
  ASSERT( pItem->IsKindOf( RUNTIME_CLASS( CObject ) ) );

  // turn into XML

  if (m_strObjectType == "trigger")
    m_doc->Save_One_Trigger_XML (ar, (CTrigger *) pItem);
  else if (m_strObjectType == "alias")
    m_doc->Save_One_Alias_XML (ar, (CAlias *) pItem);
  else if (m_strObjectType == "timer")
    m_doc->Save_One_Timer_XML (ar, (CTimer *) pItem);
  else if (m_strObjectType == "variable")
    m_doc->Save_One_Variable_XML (ar, (CVariable *) pItem);

  return false;

  } // end of CGenPropertyPage::CopyOneItem

/////////////////////////////////////////////////////////////////////////////
// OnCopyItem

void CGenPropertyPage::OnCopyItem() 
{

char * p = NULL;

try
  {
  CMemFile f;      // open memory file for writing
  CArchive ar(&f, CArchive::store);

  // in case we do more than one (one day) the header comes before the batch
  if (m_strObjectType == "alias")
    m_doc->Save_Header_XML (ar, "aliases", false);
  else
    m_doc->Save_Header_XML (ar, m_strObjectType + "s", false);   // timers, triggers etc.

  if (m_bWantTreeControl)
    { 
    for (HTREEITEM hGroup = m_cTreeCtrl.GetRootItem ();
         hGroup;
         hGroup = m_cTreeCtrl.GetNextSiblingItem (hGroup))
      {

      for (HTREEITEM hItem = m_cTreeCtrl.GetChildItem (hGroup);
          hItem;
          hItem = m_cTreeCtrl.GetNextSiblingItem (hItem))
            {
            UINT iState = m_cTreeCtrl.GetItemState (hItem, TVIS_SELECTED);

            if (iState & TVIS_SELECTED)
              {
              if (CopyOneItem ((CString *) m_cTreeCtrl.GetItemData (hItem), ar))
                {
                m_cTreeCtrl.DeleteItem (hItem);
                }
              }  // end if selected

            }   // end for each item

      }  // end for each group


    } // end of tree control
  else
    {   // list control
    for (int nItem = -1;
          (nItem = m_ctlList->GetNextItem(nItem, LVNI_SELECTED)) != -1;)
      {
      if (CopyOneItem ((CString *) m_ctlList->GetItemData (nItem), ar))
        {
        m_ctlList->DeleteItem (nItem);    // it's gone, so delete it from the list view
        m_ctlList->RedrawItems (0, m_ctlList->GetItemCount () - 1);    // redraw the list
        }
  
      }   // end of dealing with each selected item
    }  // end of list control

  if (m_strObjectType == "alias")
    m_doc->Save_Footer_XML (ar, "aliases");
  else
    m_doc->Save_Footer_XML (ar, m_strObjectType + "s");   // timers, triggers etc.

  ar.Close();

  int nLength = f.GetLength ();
  p = (char *) f.Detach ();

  CString strXML (p, nLength);

  free (p);   // remove memory allocated in CMemFile
  p = NULL;

  putontoclipboard (strXML, m_doc->m_bUTF_8);

  }   // end of try block

catch (CException* e)
	{
  if (p)
    free (p);   // remove memory allocated in CMemFile
	e->ReportError();
	e->Delete();
	}

}   // end of CGenPropertyPage::OnCopyItem

/////////////////////////////////////////////////////////////////////////////
// OnPasteItem

void CGenPropertyPage::OnPasteItem() 
  {
CString strContents; 

  if (!GetClipboardContents (strContents, m_doc->m_bUTF_8, false))
    return; // can't do it

  CMemFile f ((unsigned char *) (const char *) strContents, strContents.GetLength ());
  CArchive ar (&f, CArchive::load);
  UINT iCount = 0;

  try
    {
    iCount = m_doc->Load_World_XML (ar, m_iMask | XML_NO_PLUGINS); 
    }
  catch (CArchiveException* e) 
    {
    ::TMessageBox ("There was a problem parsing the XML on the clipboard. "
                     "See the output window for more details");
    e->Delete ();
    }

  if (iCount == 0)
    return;

  // They can no longer cancel the property sheet, the document has changed
  CancelToClose ();
  m_doc->SetModifiedFlag (TRUE);

  // reload the list - we don't know how many were added, and indeed, what they were
  LoadList ();

  } // end of CGenPropertyPage::OnPasteItem

/////////////////////////////////////////////////////////////////////////////
// EditFilterText

bool CGenPropertyPage::EditFilterText (CString & sText)// edit the filter text
  {

CEditMultiLine dlg;

  dlg.m_strText = sText;

  dlg.m_strTitle = "Edit ";
  dlg.m_strTitle += m_strObjectType;
  dlg.m_strTitle += " filter";
  
  dlg.m_bScript = true;
  dlg.m_bLua = true;

  if (dlg.DoModal () != IDOK)
      return false;

  sText = dlg.m_strText;

  return true;
  }   // end of CGenPropertyPage::EditFilterText

