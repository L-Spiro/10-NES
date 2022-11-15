#ifdef LSN_USE_WINDOWS

#pragma once

#include "../../System/LSNSystem.h"
#include <ImageList/LSWImageList.h>
#include <Images/LSWBitmap.h>
#include <MainWindow/LSWMainWindow.h>

using namespace lsw;

namespace lsw {
	class CStatusBar;
}

namespace lsn {
	
	class CMainWindow : public lsw::CMainWindow {
	public :
		CMainWindow( const LSW_WIDGET_LAYOUT &_wlLayout, CWidget * _pwParent, bool _bCreateWidget = true, HMENU _hMenu = NULL, uint64_t _ui64Data = 0 );
		~CMainWindow();


		// == Enumerations.
		// Images.
		enum LSN_IMAGES {
			LSN_I_OPENROM,
			LSN_I_OPTIONS,

			LSN_I_TOTAL,
		};


		// == Functions.
		// WM_INITDIALOG.
		virtual LSW_HANDLED						InitDialog();

		// WM_COMMAND from control.
		virtual LSW_HANDLED						Command( WORD _wCtrlCode, WORD _Id, CWidget * _pwSrc );

		// WM_COMMAND from menu.
		virtual LSW_HANDLED						MenuCommand( WORD _Id ) { return Command( 0, _Id, nullptr ); }

		// WM_NCDESTROY.
		virtual LSW_HANDLED						NcDestroy();

		/**
		 * Advances the emulation state by the amount of time that has passed since the last advancement.
		 */
		void									Tick();

		/**
		 * Virtual client rectangle.  Can be used for things that need to be adjusted based on whether or not status bars, toolbars, etc. are present.
		 *
		 * \param _pwChild Optional child control.
		 * \return Returns the virtual client rectangle of this object or of the optional child object.
		 */
		virtual const LSW_RECT					VirtualClientRect( const CWidget * _pwChild ) const;

		/**
		 * Gets the status bar.
		 *
		 * \return the status bar.
		 */
		lsw::CStatusBar *						StatusBar();

		/**
		 * Gets the status bar.
		 *
		 * \return the status bar.
		 */
		const lsw::CStatusBar *					StatusBar() const;


	protected :
		// == Members.
		// The console pointer.
		std::unique_ptr<lsn::CNtscSystem>		m_pnsSystem;
		// Image list.
		lsw::CImageList							m_iImages;
		// Images.
		lsw::CBitmap							m_bBitmaps[LSN_I_TOTAL];
		// Image mapping.
		INT										m_iImageMap[LSN_I_TOTAL];
		// Outside "is alive" atomic.
		std::atomic_bool *						m_pabIsAlive;
	};

}

#endif	// #ifdef LSN_USE_WINDOWS