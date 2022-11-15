#ifdef LSN_USE_WINDOWS

#include "LSNSelectRomDialogLayout.h"
#include "../../Utilities/LSNUtilities.h"
#include "../Layout/LSNLayoutMacros.h"
#include "../Layout/LSNLayoutManager.h"

#define LSN_OPL_W									350
#define LSN_OPL_SELECT_GROUP_TOP					LSN_TOP_JUST
#define LSN_OPL_VIEW_HEIGHT							50
#define LSN_OPL_SELECT_GROUP_HEIGHT					(LSN_OPL_VIEW_HEIGHT + LSN_GROUP_TOP + LSN_GROUP_BOTTOM)
#define LSN_OPL_SHOW_GROUP_TOP						(LSN_OPL_SELECT_GROUP_TOP + LSN_OPL_SELECT_GROUP_HEIGHT + LSN_TOP_JUST)
#define LSN_OPL_SHOW_GROUP_HEIGHT					(LSN_GROUP_TOP + LSN_GROUP_BOTTOM + LSN_DEF_RADIO_HEIGHT)
#define LSN_OPL_MAIN_CHECK_W						29
#define LSN_OPL_ALL_CHECK_W							17
#define LSN_OPL_LAST_SECTION_TOP					(LSN_OPL_SHOW_GROUP_TOP + LSN_OPL_SHOW_GROUP_HEIGHT + LSN_TOP_JUST)
#define LSN_OPL_LAST_SECTION_H						(LSN_DEF_CHECK_HEIGHT + LSN_TOP_JUST + LSN_DEF_BUTTON_HEIGHT)
#define LSN_OPL_H									(LSN_OPL_LAST_SECTION_TOP + LSN_OPL_LAST_SECTION_H + LSN_TOP_JUST * 4 + LSN_DEF_BUTTON_HEIGHT)

namespace lsn {

		// == Members.
	// The layout for the Open Process dialog.
	LSW_WIDGET_LAYOUT CSelectRomDialogLayout::m_wlSelectRomDialog[] = {
		{
			LSN_LT_SELECT_ROM_DIALOG,				// ltType
			LSN_SFI_DIALOG,							// wId
			nullptr,								// lpwcClass
			TRUE,									// bEnabled
			FALSE,									// bActive
			64,										// iLeft
			64,										// iTop
			LSN_OPL_W,								// dwWidth
			LSN_OPL_H,								// dwHeight
			WS_CAPTION | WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_SYSMENU | WS_THICKFRAME | DS_3DLOOK | DS_FIXEDSYS | DS_CENTER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,					// dwStyle
			//WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION,
			WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_WINDOWEDGE | WS_EX_CONTROLPARENT,												// dwStyleEx
			L"Select ROM",							// pwcText
			0,										// sTextLen
			LSN_SFI_NONE,							// dwParentId
		},

		// Select ROM.
		{
			LSW_LT_GROUPBOX,						// ltType
			LSN_SFI_GROUP,							// wId
			WC_BUTTONW,								// lpwcClass
			TRUE,									// bEnabled
			FALSE,									// bActive
			LSN_LEFT_JUST,							// iLeft
			LSN_OPL_SELECT_GROUP_TOP,				// iTop
			LSN_OPL_W - (LSN_LEFT_JUST * 2),		// dwWidth
			LSN_OPL_SELECT_GROUP_HEIGHT,			// dwHeight
			LSN_GROUPSTYLE,																																// dwStyle
			WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY,																// dwStyleEx
			L"File Listing",						// pwcText
			0,										// sTextLen
			LSN_SFI_DIALOG,							// dwParentId

			LSN_LOCK_LEFT,							// pcLeftSizeExp
			LSN_LOCK_RIGHT,							// pcRightSizeExp
			LSN_LOCK_TOP,							// pcTopSizeExp
			LSN_LOCK_BOTTOM,						// pcBottomSizeExp
			nullptr, 0,								// pcWidthSizeExp
			nullptr, 0,								// pcHeightSizeExp
		},
		{
			LSW_LT_LISTBOX,							// ltType
			LSN_SFI_LISTBOX,						// wId
			WC_LISTBOXW,							// lpwcClass
			TRUE,									// bEnabled
			TRUE,									// bActive
			LSN_LEFT_JUST + LSN_GROUP_LEFT,			// iLeft
			LSN_OPL_SELECT_GROUP_TOP + LSN_GROUP_TOP,	// iTop
			LSN_OPL_W - (LSN_LEFT_JUST + LSN_GROUP_LEFT) * 2,																								// dwWidth
			LSN_OPL_VIEW_HEIGHT,					// dwHeight
			WS_CHILDWINDOW | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,						// dwStyle
			WS_EX_LEFT | WS_EX_LTRREADING | WS_EX_RIGHTSCROLLBAR | WS_EX_NOPARENTNOTIFY | WS_EX_CLIENTEDGE,		// dwStyleEx
			nullptr,								// pwcText
			0,										// sTextLen
			LSN_SFI_DIALOG,							// dwParentId

			LSN_LOCK_LEFT,							// pcLeftSizeExp
			LSN_LOCK_RIGHT,							// pcRightSizeExp
			LSN_LOCK_TOP,							// pcTopSizeExp
			LSN_LOCK_BOTTOM,						// pcBottomSizeExp
			nullptr, 0,								// pcWidthSizeExp
			nullptr, 0,								// pcHeightSizeExp
		},
	};

#undef LSN_OPL_H
#undef LSN_OPL_LAST_SECTION_H
#undef LSN_OPL_LAST_SECTION_TOP
#undef LSN_OPL_ALL_CHECK_W
#undef LSN_OPL_MAIN_CHECK_W
#undef LSN_OPL_SHOW_GROUP_HEIGHT
#undef LSN_OPL_SHOW_GROUP_TOP
#undef LSN_OPL_SELECT_GROUP_HEIGHT
#undef LSN_OPL_VIEW_HEIGHT
#undef LSN_OPL_SELECT_GROUP_TOP
#undef LSN_OPL_W

	// == Functions.
	// Creates the Select ROM dialog.
	DWORD CSelectRomDialogLayout::CreateSelectRomDialog( CWidget * _pwParent, const std::vector<std::u16string> * _pvFiles ) {
		lsn::CLayoutManager * plmLayout = static_cast<lsn::CLayoutManager *>(lsw::CBase::LayoutManager());
		INT_PTR ipProc = plmLayout->DialogBoxX( m_wlSelectRomDialog, LSN_ELEMENTS( m_wlSelectRomDialog ), _pwParent, reinterpret_cast<uint64_t>(_pvFiles) );

		return static_cast<DWORD>(ipProc);
	}

}	// namespace lsn

#endif	// #ifdef LSN_USE_WINDOWS
