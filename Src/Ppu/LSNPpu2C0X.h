/**
 * Copyright L. Spiro 2022
 *
 * Written by: Shawn (L. Spiro) Wilcoxen
 *
 * Description: The 2C0X series of PPU's.
 */


#pragma once

#include "../LSNLSpiroNes.h"
#include "../Bus/LSNBus.h"
#include "../Display/LSNDisplayClient.h"
#include "../Display/LSNDisplayHost.h"
#include "../Mappers/LSNMapperBase.h"
#include "../Palette/LSNPalette.h"
#include "../System/LSNNmiable.h"
#include "../System/LSNTickable.h"

#define LSN_CTRL_NAMETABLE_X( OBJ )						(OBJ.s.ui8Nametable & 0x01)
#define LSN_CTRL_NAMETABLE_Y( OBJ )						((OBJ.s.ui8Nametable >> 1) & 0x01)

#define LSN_USE_PPU_DIAGRAM_CYCLE_SKIP

#define LSN_END_CONTROL_CYCLE							++m_ui16RowDot;

#define LSN_TEMPLATE_PPU
//#define LSN_GEN_PPU
//#define LSN_PPU_GEN_DEBUG

#ifdef LSN_GEN_PPU
#include <map>
#include <string>
#include <vector>
#endif	// #ifdef LSN_GEN_PPU

namespace lsn {

	/**
	 * Class CPpu2C0X
	 * \brief The 2C0X series of PPU's.
	 *
	 * Description: The 2C0X series of PPU's.
	 */
	template <unsigned _tRegCode,
		unsigned _tDotWidth, unsigned _tDotHeight,
		unsigned _tPreRender, unsigned _tRender, unsigned _tPostRender,
		unsigned _tRenderW, unsigned _tBorderW,
		bool _bOddFrameShenanigans, double _dPerferredRatio>
	class CPpu2C0X : public CTickable, public CDisplayClient {
	public :
		CPpu2C0X( CCpuBus * _pbBus, CNmiable * _pnNmiTarget ) :
			m_pbBus( _pbBus ),
			m_pnNmiTarget( _pnNmiTarget ),
			m_ui64Frame( 0 ),
			m_ui64Cycle( 0 ),
			m_ui16Scanline( 0 ),
			m_ui16RowDot( 0 ),
			m_ui16ShiftPatternLo( 0 ),
			m_ui16ShiftPatternHi( 0 ),
			m_ui16ShiftAttribLo( 0 ),
			m_ui16ShiftAttribHi( 0 ),
			m_ui8ThisLineSpriteCount( 0 ),
			m_ui8NtAtBuffer( 0 ),
			m_ui8NextTileId( 0 ),
			m_ui8NextTileAttribute( 0 ),
			m_ui8NextTileLsb( 0 ),
			m_ui8NextTileMsb( 0 ),
			m_ui8IoBusLatch( 0 ),
			m_ui8OamAddr( 0 ),
			m_ui8OamLatch( 0 ),
			m_ui8Oam2ClearIdx( 0 ),
			m_bAddresLatch( false ) {

#ifdef LSN_TEMPLATE_PPU
#ifdef LSN_GEN_PPU
			GenerateCycleFuncs();
#else
#include "LSNCreateCycleTableNtsc.inl"
#include "LSNCreateCycleTablePal.inl"
#include "LSNCreateCycleTableDendy.inl"
#endif	// #ifdef LSN_GEN_PPU
#endif	// #ifdef LSN_TEMPLATE_PPU

			for ( auto Y = _tDotHeight; Y--; ) {
				for ( auto X = _tDotWidth; X--; ) {
					{
						LSN_CYCLE & cThis = m_cControlCycles[Y*_tDotWidth+X];
						cThis.pfFunc = &CPpu2C0X::Pixel_Idle_Control;
					}
					{
						LSN_CYCLE & cThis = m_cWorkCycles[Y*_tDotWidth+X];
						cThis.pfFunc = &CPpu2C0X::Pixel_Idle_Work;
					}
					{
						LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+X];
						cThis.pfFunc = &CPpu2C0X::Pixel_Idle_Sprite;
					}
				}
			}
			// Add pixel gather/render functions.
			{
				// The main rendering area.

				for ( auto Y = 0; Y < (_tPreRender + _tRender); ++Y ) {
					ApplyStdRenderFunctionPointers( uint16_t( Y ) );
					ApplySpriteFunctionPointers( uint16_t( Y ) );
				}
				// The "-1" scanline.
				{
					constexpr size_t Y = _tDotHeight - 1;
					ApplyStdRenderFunctionPointers( uint16_t( Y ) );
					//ApplySpriteFunctionPointers( uint16_t( Y ) );
				}

			}


			// Add row ends.
			for ( auto Y = _tDotHeight; Y--; ) {
				LSN_CYCLE & cThis = m_cControlCycles[Y*_tDotWidth+(_tDotWidth-1)];
				cThis.pfFunc = &CPpu2C0X::Pixel_Idle_EndRow_Control;
			}
			// Add frame end.
			{
#ifdef LSN_USE_PPU_DIAGRAM_CYCLE_SKIP
				LSN_CYCLE & cThis = m_cControlCycles[(_tDotHeight-1)*_tDotWidth+(_tDotWidth-1)];
				cThis.pfFunc = &CPpu2C0X::Pixel_Idle_EndFrame_Control;
#else
				{
					LSN_CYCLE & cThis = m_cControlCycles[(_tDotHeight-1)*_tDotWidth+(_tDotWidth-2)];
					cThis.pfFunc = &CPpu2C0X::Pixel_Idle_EndFrame_Control;
				}
				{
					LSN_CYCLE & cThis = m_cControlCycles[(_tDotHeight-1)*_tDotWidth+(_tDotWidth-1)];
					cThis.pfFunc = &CPpu2C0X::Pixel_Idle_EndFrame_Even_Control;
				}
#endif
				
			}
			// Add v-blank.
			{
				LSN_CYCLE & cThis = m_cControlCycles[(_tPreRender+_tRender+_tPostRender)*_tDotWidth+1];
				cThis.pfFunc = &CPpu2C0X::Pixel_Idle_VBlank_Control;
			}
			// Clear v-blank and friends.
			{
				LSN_CYCLE & cThis = m_cControlCycles[(_tDotHeight-1)*_tDotWidth+1];
				cThis.pfFunc = &CPpu2C0X::Pixel_Idle_StartFrame_Control;
			}
			// Swap the display buffer.
			{
				LSN_CYCLE & cThis = m_cControlCycles[(_tPreRender+_tRender)*_tDotWidth+(1+_tRenderW+2)];
				cThis.pfFunc = &CPpu2C0X::Pixel_Swap_Control;
			}
		}
		~CPpu2C0X() {
		}


		// == Types.
		/** Sprite-evaluation stages. */
		enum LSN_SPRITE_EVAL_STATE {
			LSN_SES_CHECK_NEXT,							/**< Copies the Y from the next OAM sprite. */
			LSN_SES_ADD_SPRITE,							/**< Copies the remaining 3 bytes from the OAM sprite to the secondary OAM sprite. */
			LSN_SES_OF_SEARCH_ADD_SPRITE,				/**< Reads the remaining 3 bytes of the OAM sprite and then goes to LSN_SES_FINISHED_OAM_LIST. */
			LSN_SES_FINISHED_OAM_LIST,					/**< When evaluation has gone through all of the OAM sprites. */
		};


		// == Functions.
		/**
		 * Performs a single cycle update.
		 */
		virtual void									Tick() {
#ifdef LSN_PPU_GEN_DEBUG
			LSN_DEBUG_COPY dcAtStart = {
				m_ui64Frame,
				m_paPpuAddrT, m_paPpuAddrV, m_psPpuStatus, m_stCurCycle / _tDotWidth, m_stCurCycle % _tDotWidth,
				m_ui16ShiftPatternLo, m_ui16ShiftPatternHi, m_ui16ShiftAttribLo, m_ui16ShiftAttribHi,
				m_ui8FineScrollX, m_ui8NtAtBuffer,
				m_ui8NextTileId, m_ui8NextTileAttribute, m_ui8NextTileLsb, m_ui8NextTileMsb
			};

			size_t stThisIdx = m_stCurCycle;
			(this->*m_cCycle[stThisIdx])();
			LSN_DEBUG_COPY dcAfter = {
				m_ui64Frame,
				m_paPpuAddrT, m_paPpuAddrV, m_psPpuStatus, m_stCurCycle / _tDotWidth, m_stCurCycle % _tDotWidth,
				m_ui16ShiftPatternLo, m_ui16ShiftPatternHi, m_ui16ShiftAttribLo, m_ui16ShiftAttribHi,
				m_ui8FineScrollX, m_ui8NtAtBuffer,
				m_ui8NextTileId, m_ui8NextTileAttribute, m_ui8NextTileLsb, m_ui8NextTileMsb
			};

			m_ui64Frame = dcAtStart.m_ui64Frame;

			m_paPpuAddrT = dcAtStart.m_paPpuAddrT;
			m_paPpuAddrV = dcAtStart.m_paPpuAddrV;
			m_psPpuStatus = dcAtStart.m_psPpuStatus;

			m_ui16ShiftPatternLo = dcAtStart.m_ui16ShiftPatternLo;
			m_ui16ShiftPatternHi = dcAtStart.m_ui16ShiftPatternHi;
			m_ui16ShiftAttribLo = dcAtStart.m_ui16ShiftAttribLo;
			m_ui16ShiftAttribHi = dcAtStart.m_ui16ShiftAttribHi;

			m_ui8FineScrollX = dcAtStart.m_ui8FineScrollX;
			m_ui8NtAtBuffer = dcAtStart.m_ui8NtAtBuffer;

			m_ui8NextTileId = dcAtStart.m_ui8NextTileId;
			m_ui8NextTileAttribute = dcAtStart.m_ui8NextTileAttribute;
			m_ui8NextTileLsb = dcAtStart.m_ui8NextTileLsb;
			m_ui8NextTileMsb = dcAtStart.m_ui8NextTileMsb;

			size_t stIdx = m_ui16Scanline * _tDotWidth + m_ui16RowDot;
			/*if ( m_ui16RowDot == 321 ) {
				int gjhg = 0;
			}*/
			//(this->*m_cSpriteCycles[stIdx].pfFunc)();
			(this->*m_cWorkCycles[stIdx].pfFunc)();
			(this->*m_cControlCycles[stIdx].pfFunc)();

			if ( m_ui64Frame != dcAfter.m_ui64Frame ) {
				volatile int gjhg =0;
			}
			if ( m_paPpuAddrT.ui16Addr != dcAfter.m_paPpuAddrT.ui16Addr ) {
				volatile int gjhg =0;
			}
			if ( m_paPpuAddrV.ui16Addr != dcAfter.m_paPpuAddrV.ui16Addr ) {
				volatile int gjhg =0;
			}
			if ( m_psPpuStatus.ui8Reg != dcAfter.m_psPpuStatus.ui8Reg ) {
				volatile int gjhg =0;
			}
			if ( m_ui16Scanline != dcAfter.m_ui16Scanline ) {
				volatile int gjhg =0;
			}
			if ( m_ui16RowDot != dcAfter.m_ui16RowDot ) {
				volatile int gjhg =0;
			}


			if ( m_ui16ShiftPatternLo != dcAfter.m_ui16ShiftPatternLo ) {
				volatile int gjhg =0;
			}
			if ( m_ui16ShiftPatternHi != dcAfter.m_ui16ShiftPatternHi ) {
				volatile int gjhg =0;
			}
			if ( m_ui16ShiftAttribLo != dcAfter.m_ui16ShiftAttribLo ) {
				volatile int gjhg =0;
			}
			if ( m_ui16ShiftAttribHi != dcAfter.m_ui16ShiftAttribHi ) {
				volatile int gjhg =0;
			}

			if ( m_ui8FineScrollX != dcAfter.m_ui8FineScrollX ) {
				volatile int gjhg =0;
			}
			if ( m_ui8NtAtBuffer != dcAfter.m_ui8NtAtBuffer ) {
				volatile int gjhg =0;
			}

			if ( m_ui8NextTileId != dcAfter.m_ui8NextTileId ) {
				volatile int gjhg =0;
			}
			if ( m_ui8NextTileAttribute != dcAfter.m_ui8NextTileAttribute ) {
				volatile int gjhg =0;
			}
			if ( m_ui8NextTileLsb != dcAfter.m_ui8NextTileLsb ) {
				volatile int gjhg =0;
			}
			if ( m_ui8NextTileMsb != dcAfter.m_ui8NextTileMsb ) {
				volatile int gjhg =0;
			}
#else
#ifdef LSN_TEMPLATE_PPU
			(this->*m_cCycle[m_stCurCycle])();
#else
			size_t stIdx = m_ui16Scanline * _tDotWidth + m_ui16RowDot;
			//(this->*m_cSpriteCycles[stIdx].pfFunc)();
			(this->*m_cWorkCycles[stIdx].pfFunc)();
			(this->*m_cControlCycles[stIdx].pfFunc)();
#endif	// #ifdef LSN_TEMPLATE_PPU
#endif	// #ifdef LSN_PPU_GEN_DEBUG
			++m_ui64Cycle;
		}

		/**
		 * Resets the CPU to a known state.
		 */
		void											ResetToKnown() {
			ResetAnalog();
			m_ui64Frame = 0;
			m_ui64Cycle = 0;
			m_ui16Scanline = 0;
			m_ui16RowDot = 0;
			m_paPpuAddrT.ui16Addr = 0;
			m_paPpuAddrV.ui16Addr = 0;
			m_ui8IoBusLatch = 0;

#ifdef LSN_TEMPLATE_PPU
			m_stCurCycle = 0;
#endif	// #ifdef LSN_TEMPLATE_PPU
		}

		/**
		 * Performs an "analog" reset, allowing previous data to remain.
		 */
		void											ResetAnalog() {
			m_pcPpuCtrl.ui8Reg = 0;

			m_pmPpuMask.ui8Reg = 0;

			m_psPpuStatus.s.ui8SpriteOverflow = 0;
			m_psPpuStatus.s.ui8Sprite0Hit = 0;
			m_psPpuStatus.s.ui8VBlank = 0;

			m_bAddresLatch = false;
			m_ui8FineScrollX = 0;

			m_ui8OamAddr = 0;
			m_ui8OamLatch = 0;
			m_ui8Oam2ClearIdx = 0;
		}

		/**
		 * Applies the PPU's memory mapping t the bus.
		 */
		void											ApplyMemoryMap() {
			// == Pattern Tables
			for ( uint32_t I = LSN_PPU_PATTERN_TABLES; I < LSN_PPU_NAMETABLES; ++I ) {
				m_bBus.SetReadFunc( uint16_t( I ), CPpuBus::StdRead, this, uint16_t( ((I - LSN_PPU_PATTERN_TABLES) % LSN_PPU_PATTERN_TABLE_SIZE) + LSN_PPU_PATTERN_TABLES ) );
				m_bBus.SetWriteFunc( uint16_t( I ), CPpuBus::StdWrite, this, uint16_t( ((I - LSN_PPU_PATTERN_TABLES) % LSN_PPU_PATTERN_TABLE_SIZE) + LSN_PPU_PATTERN_TABLES ) );
			}

			// == Nametables
			ApplyVerticalMirroring();

			// == Palettes
			for ( uint32_t I = LSN_PPU_PALETTE_MEMORY; I < LSN_PPU_MEM_FULL_SIZE; ++I ) {
				m_bBus.SetReadFunc( uint16_t( I ), PaletteRead, this, uint16_t( ((I - LSN_PPU_PALETTE_MEMORY) % LSN_PPU_PALETTE_MEMORY_SIZE) + LSN_PPU_PALETTE_MEMORY ) );
				m_bBus.SetWriteFunc( uint16_t( I ), CPpuBus::StdWrite, this, uint16_t( ((I - LSN_PPU_PALETTE_MEMORY) % LSN_PPU_PALETTE_MEMORY_SIZE) + LSN_PPU_PALETTE_MEMORY ) );
			}
			// 4th color of each entry mirrors the background color at LSN_PPU_PALETTE_MEMORY.
			for ( uint32_t I = LSN_PPU_PALETTE_MEMORY + 4; I < LSN_PPU_MEM_FULL_SIZE; I += 4 ) {
				/*m_bBus.SetReadFunc( uint16_t( I ), CCpuBus::StdRead, this, uint16_t( ((I - LSN_PPU_PALETTE_MEMORY) % (LSN_PPU_PALETTE_MEMORY_SIZE / 2)) + LSN_PPU_PALETTE_MEMORY ) );
				m_bBus.SetWriteFunc( uint16_t( I ), CCpuBus::StdWrite, this, uint16_t( ((I - LSN_PPU_PALETTE_MEMORY) % (LSN_PPU_PALETTE_MEMORY_SIZE / 2)) + LSN_PPU_PALETTE_MEMORY ) );*/
				m_bBus.SetReadFunc( uint16_t( I ), PaletteRead, this, uint16_t( LSN_PPU_PALETTE_MEMORY ) );
				m_bBus.SetWriteFunc( uint16_t( I ), WritePaletteIdx4, this, uint16_t( ((I - LSN_PPU_PALETTE_MEMORY) % (LSN_PPU_PALETTE_MEMORY_SIZE / 2)) + LSN_PPU_PALETTE_MEMORY ) );
			}

			for ( uint32_t I = LSN_PPU_START; I < LSN_APU_START; I += LSN_PPU ) {
				// 0x2000: PPUCTRL.
				m_pbBus->SetReadFunc( uint16_t( I + 0x00 ), PpuNoRead, this, LSN_PPU_START );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x00 ), Write2000, this, 0 );

				// 0x2001: PPUMASK.
				m_pbBus->SetReadFunc( uint16_t( I + 0x01 ), PpuNoRead, this, LSN_PPU_START + 0x01 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x01 ), Write2001, this, 0 );

				// 0x2002: PPUSTATUS.
				m_pbBus->SetReadFunc( uint16_t( I + 0x02 ), Read2002, this, 0 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x02 ), Write2002, this, 0 );

				// 0x2003: OAMADDR.
				m_pbBus->SetReadFunc( uint16_t( I + 0x03 ), PpuNoRead, this, LSN_PPU_START + 0x03 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x03 ), Write2003, this, 0 );

				// 0x2004: OAMDATA.
				m_pbBus->SetReadFunc( uint16_t( I + 0x04 ), Read2004, this, 0 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x04 ), Write2004, this, 0 );

				// 0x2005: PPUSCROLL.
				m_pbBus->SetReadFunc( uint16_t( I + 0x05 ), PpuNoRead, this, LSN_PPU_START + 0x05 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x05 ), Write2005, this, 0 );

				// 0x2006: PPUADDR.
				m_pbBus->SetReadFunc( uint16_t( I + 0x06 ), PpuNoRead, this, LSN_PPU_START + 0x06 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x06 ), Write2006, this, 0 );

				// 0x2007: PPUDATA.
				m_pbBus->SetReadFunc( uint16_t( I + 0x07 ), Read2007, this, 0 );
				m_pbBus->SetWriteFunc( uint16_t( I + 0x07 ), Write2007, this, 0 );
			}
		}

		/**
		 * Applies vertical mirroring to the nametable addresses ([LSN_PPU_NAMETABLES..LSN_PPU_PALETTE_MEMORY]).
		 */
		void											ApplyVerticalMirroring() {
			CMapperBase::ApplyMirroring( LSN_MM_VERTICAL, &m_bBus, this );
			::OutputDebugStringA( "****** LSN_MM_VERTICAL.\r\n" );
		}

		/**
		 * Applies horizontal mirroring to the nametable addresses ([LSN_PPU_NAMETABLES..LSN_PPU_PALETTE_MEMORY]).
		 */
		void											ApplyHorizontalMirroring() {
			CMapperBase::ApplyMirroring( LSN_MM_HORIZONTAL, &m_bBus, this );
			::OutputDebugStringA( "****** LSN_MM_HORIZONTAL.\r\n" );
		}

		/**
		 * Applies 4-screens mirroring to the nametable addresses ([LSN_PPU_NAMETABLES..LSN_PPU_PALETTE_MEMORY]).
		 */
		void											ApplyFourScreensMirroring() {
			CMapperBase::ApplyMirroring( LSN_MM_4_SCREENS, &m_bBus, this );
			::OutputDebugStringA( "****** LSN_MM_4_SCREENS.\r\n" );
		}

		/**
		 * Applies 1-screen mirroring to the nametable addresses ([LSN_PPU_NAMETABLES..LSN_PPU_PALETTE_MEMORY]).
		 */
		void											ApplyOneScreenMirroring() {
			CMapperBase::ApplyMirroring( LSN_MM_1_SCREEN_A, &m_bBus, this );
			::OutputDebugStringA( "****** LSN_MM_1_SCREEN_A.\r\n" );
		}

		/**
		 * Applies 1-screen mirroring to the nametable addresses ([LSN_PPU_NAMETABLES..LSN_PPU_PALETTE_MEMORY]).
		 */
		void											ApplyOneScreenMirroring_B() {
			CMapperBase::ApplyMirroring( LSN_MM_1_SCREEN_B, &m_bBus, this );
			::OutputDebugStringA( "****** LSN_MM_1_SCREEN_B.\r\n" );
		}

		/**
		 * Gets the full X resolution, including off-screen areas.
		 *
		 * \return Returns the full X resolution, including off-screen areas.
		 */
		inline constexpr uint16_t						DotWidth() const { return _tDotWidth; }

		/**
		 * Gets the full Y resolution, including off-screen areas.
		 *
		 * \return Returns the full Y resolution, including off-screen areas.
		 */
		inline constexpr uint16_t						DotHeight() const { return _tDotHeight; }

		/**
		 * Gets the frame count.
		 *
		 * \return Returns the frame count.
		 */
		inline uint64_t									GetFrameCount() const { return m_ui64Frame; }

		/**
		 * Gets the cycle count.
		 *
		 * \return Returns the cycle count.
		 */
		inline uint64_t									GetCycleCount() const { return m_ui64Cycle; }

		/**
		 * Gets the current row position.
		 *
		 * \return Returns the current row position.
		 */
		inline uint16_t									GetCurrentRowPos() const { return m_ui16RowDot; }

		/**
		 * Gets the current scanline.
		 *
		 * \return Returns the current scanline.
		 */
		inline uint16_t									GetCurrentScanline() const { return m_ui16Scanline; }

		/**
		 * Gets the PPU bus.
		 *
		 * \return Returns the PPU bus.
		 */
		inline CPpuBus &								GetPpuBus() { return m_bBus; }

		/**
		 * Gets the display width in pixels.  Used to create render targets.
		 *
		 * \return Returns the pixel width of the display area.
		 */
		virtual uint32_t								DisplayWidth() const { return _tRenderW; }

		/**
		 * Gets the display height in pixels.  Used to create render targets.
		 *
		 * \return Returns the pixel height of the display area.
		 */
		virtual uint32_t								DisplayHeight() const { return _tRender + _tPreRender; }

		/**
		 * Gets the display ratio in pixels.
		 *
		 * \return Returns the ratio of the display area.
		 */
		virtual double									DisplayRatio() const { return _dPerferredRatio; }

		/**
		 * If true, extra room is added to the side of the view to display some debug information.
		 *
		 * \return Returns true in order to add an extra 128 pixels horizontally for debug display, otherwise false.  Defaults to false.
		 */
		virtual bool									DebugSideDisplay() const { return false; }

		/**
		 * Gets a reference to the PPU bus.
		 *
		 * \return Returns a reference to the PPU bus.
		 */
		CPpuBus &										GetBus() { return m_bBus; }

		/**
		 * Gets the palette.
		 *
		 * \return Returns a reference to the palette.
		 */
		LSN_PALETTE &									Palette() { return m_pPalette; }

		/**
		 * Gets the palette as read-only.
		 *
		 * \return Returns a constant reference to the palette.
		 */
		const LSN_PALETTE &								Palette() const { return m_pPalette; }

		/**
		 * An "idle" pixel handler.
		 */
		void LSN_FASTCALL								Pixel_Idle_Control() {
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * An "idle" work pixel handler.  Does nothing.
		 */
		void LSN_FASTCALL								Pixel_Idle_Work() {
		}

		/**
		 * An "idle" sprite pixel handler.  Does nothing.
		 */
		void LSN_FASTCALL								Pixel_Idle_Sprite() {
		}

		/**
		 * An "idle" pixel handler for the end of rows.
		 */
		void LSN_FASTCALL								Pixel_Idle_EndRow_Control() {
			m_ui16RowDot = 0;
			++m_ui16Scanline;
		}

		/**
		 * Starting a frame at [1,_tDotHeight-1] clears some flags.
		 */
		void LSN_FASTCALL								Pixel_Idle_StartFrame_Control() {
			m_psPpuStatus.s.ui8VBlank = 0;
			m_psPpuStatus.s.ui8SpriteOverflow = 0;
			m_psPpuStatus.s.ui8Sprite0Hit = 0;
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * An "idle" pixel handler for the end of the frame.
		 */
		void LSN_FASTCALL								Pixel_Idle_EndFrame_Control() {
#ifdef LSN_USE_PPU_DIAGRAM_CYCLE_SKIP
			m_ui16Scanline = 0;
			
			if constexpr ( _bOddFrameShenanigans ) {
				if ( m_ui64Frame & 0x1 ) {
					m_ui16RowDot = 1;
				}
				else {
					m_ui16RowDot = 0;
				}
			}
			else {
				m_ui16RowDot = 0;
			}
			++m_ui64Frame;
#else
			if constexpr ( _bOddFrameShenanigans ) {
				if ( (m_ui64Frame & 0x1) && Rendering() ) {
					// Skips Pixel_Idle_EndFrame_Even_Control().
					m_ui16Scanline = 0;
					m_ui16RowDot = 0;
					++m_ui64Frame;
				}
				else {
					// Goes to Pixel_Idle_EndFrame_Even_Control().
					LSN_END_CONTROL_CYCLE;
				}
			}
			else {
				// Goes to Pixel_Idle_EndFrame_Even_Control().
				LSN_END_CONTROL_CYCLE;
			}
#endif	// #ifdef LSN_USE_PPU_DIAGRAM_CYCLE_SKIP
		}

		/**
		 * An "idle" pixel handler for the end of even frames.  Skipped on odd frames if rendering is off.
		 */
		void LSN_FASTCALL								Pixel_Idle_EndFrame_Even_Control() {
			m_ui16Scanline = 0;
			m_ui16RowDot = 0;
			++m_ui64Frame;
		}

		/**
		 * Handling v-blank.
		 */
		void LSN_FASTCALL								Pixel_Idle_VBlank_Control() {
			// [1, 241] on NTSC.
			// [1, 241] on PAL.
			// [1, 241] on Dendy.
			m_psPpuStatus.s.ui8VBlank = 1;
			if ( m_pcPpuCtrl.s.ui8Nmi ) {
				m_pnNmiTarget->Nmi();
			}
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * Informing the display host that a render is done.
		 */
		void LSN_FASTCALL								Pixel_Swap_Control() {
			if ( m_pdhHost ) {
				if ( m_pui8RenderTarget ) {
					
					if ( DebugSideDisplay() ) {
						for ( uint16_t I = 0; I < 2; ++I ) {
							for ( uint16_t ui16TileY = 0; ui16TileY < 16; ++ui16TileY ) {
								for ( uint16_t ui16TileX = 0; ui16TileX < 16; ++ui16TileX ) {
									uint16_t ui16Offset = ui16TileY * 256 + ui16TileX * 16;
									for ( uint16_t ui16Row = 0; ui16Row < 8; ++ui16Row ) {
										uint8_t ui8TileLsb = m_bBus.Read( 0x1000 * I + ui16Offset + ui16Row + 0 );
										uint8_t ui8TileMsb = m_bBus.Read( 0x1000 * I + ui16Offset + ui16Row + 8 );
										for ( uint16_t ui16Col = 0; ui16Col < 8; ++ui16Col ) {
											uint8_t ui8Pixel = (ui8TileLsb & 0x01) + (ui8TileMsb & 0x01);
											ui8TileLsb >>= 1;
											ui8TileMsb >>= 1;
											uint16_t ui16X = ui16TileX * 8 + (7 - ui16Col);
											ui16X += _tRenderW;

											uint16_t ui16Y = I * (16 * 8) + ui16TileY * 8 + ui16Row;
											ui16Y = (_tRender - 1) - ui16Y;
											if ( ui16Y < _tRender ) {

												uint8_t * pui8This = &m_pui8RenderTarget[ui16Y*m_stRenderTargetStride+ui16X*3];
												uint8_t ui8Val = ui8Pixel * (255 / 4);
												pui8This[0] = pui8This[1] = pui8This[2] = ui8Val;
											}
										}
									}
								}
							}
						}
					}
#if 0
					// For fun.
					for ( auto Y = DisplayHeight(); Y--; ) {
						for ( auto X = DisplayWidth(); X--; ) {
							uint8_t * pui8This = &m_pui8RenderTarget[Y*m_stRenderTargetStride+X*3];
							uint16_t ui16Addr = uint16_t( (Y + m_ui64Frame) * 256 + X * 1 );
							
							uint8_t ui8Val = m_pbBus->DBG_Inspect( ui16Addr + 0 );
							pui8This[0] = pui8This[1] = pui8This[2] = ui8Val;
							/*pui8This[2] = m_pbBus->DBG_Inspect( ui16Addr + 1 );
							pui8This[0] = m_pbBus->DBG_Inspect( ui16Addr + 0 );*/
						}
					}
#endif
				}
				m_pdhHost->Swap();
			}
#if !defined( LSN_TEMPLATE_PPU ) || defined( LSN_PPU_GEN_DEBUG )
			LSN_END_CONTROL_CYCLE;
#endif	// #ifndef LSN_TEMPLATE_PPU
		}

		/**
		 * The first of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadNtNoShift_0_Work() {
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );
		}

		/**
		 * The second of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadNtNoShift_1_Work() {
			m_ui8NextTileId = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadNt_0_Work() {
			ShiftBackgroundAndSpriteRegisters();
			LoadLatchedBackgroundIntoShiftRegisters();
			RenderPixel();
			// LSN_PPU_NAMETABLES = 0x2000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );
		}

		/**
		 * The second of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadNt_1_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			m_ui8NextTileId = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 AT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadAt_0_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			// LSN_PPU_NAMETABLES = 0x2000.
			// LSN_PPU_ATTRIBUTE_TABLE_OFFSET = 0x03C0.
			m_ui8NtAtBuffer = m_bBus.Read( (LSN_PPU_NAMETABLES + LSN_PPU_ATTRIBUTE_TABLE_OFFSET) | (m_paPpuAddrV.s.ui16NametableY << 11) |
				(m_paPpuAddrV.s.ui16NametableX << 10) |
				((m_paPpuAddrV.s.ui16CourseY >> 2) << 3) |
				(m_paPpuAddrV.s.ui16CourseX >> 2) );
			if ( m_paPpuAddrV.s.ui16CourseY & 0x2 ) { m_ui8NtAtBuffer >>= 4; }
			if ( m_paPpuAddrV.s.ui16CourseX & 0x2 ) { m_ui8NtAtBuffer >>= 2; }
			m_ui8NtAtBuffer &= 0x3;
		}

		/**
		 * The second of the 2 AT read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadAt_1_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			m_ui8NextTileAttribute = m_ui8NtAtBuffer;
			/*if ( m_paPpuAddrV.s.ui16CourseY & 0x2 ) { m_ui8NextTileAttribute >>= 4; }
			if ( m_paPpuAddrV.s.ui16CourseX & 0x2 ) { m_ui8NextTileAttribute >>= 2; }
			m_ui8NextTileAttribute &= 0x3;*/
		}

		/**
		 * The first of the 2 LSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadLsb_0_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			// LSN_PPU_PATTERN_TABLES = 0x0000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +
				(static_cast<uint16_t>(m_ui8NextTileId) << 4) +
				(m_paPpuAddrV.s.ui16FineY) +
				0) );
		}

		/**
		 * The second of the 2 LSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadLsb_1_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			m_ui8NextTileLsb = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 MSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadMsb_0_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			// LSN_PPU_PATTERN_TABLES = 0x0000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +
				(static_cast<uint16_t>(m_ui8NextTileId) << 4) +
				(m_paPpuAddrV.s.ui16FineY) +
				8) );
		}

		/**
		 * The second of the 2 MSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_LoadMsb_1_Work() {
			ShiftBackgroundAndSpriteRegisters();
			RenderPixel();
			m_ui8NextTileMsb = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadNt_0_Work() {
			m_ui8OamAddr = 0;
			// LSN_PPU_NAMETABLES = 0x2000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );
		}

		/**
		 * The second of the 2 NT read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadNt_1_Work() {
			m_ui8OamAddr = 0;
			m_ui8NextTileId = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 AT read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadAt_0_Work() {
			m_ui8OamAddr = 0;
			/*// LSN_PPU_NAMETABLES = 0x2000.
			// LSN_PPU_ATTRIBUTE_TABLE_OFFSET = 0x03C0.
			m_ui8NtAtBuffer = m_bBus.Read( (LSN_PPU_NAMETABLES + LSN_PPU_ATTRIBUTE_TABLE_OFFSET) | (m_paPpuAddrV.s.ui16NametableY << 11) |
				(m_paPpuAddrV.s.ui16NametableX << 10) |
				((m_paPpuAddrV.s.ui16CourseY >> 2) << 3) |
				(m_paPpuAddrV.s.ui16CourseX >> 2) );*/
			// LSN_PPU_NAMETABLES = 0x2000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );
		}

		/**
		 * The second of the 2 AT read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadAt_1_Work() {
			m_ui8OamAddr = 0;
			//m_ui8NextTileAttribute = m_ui8NtAtBuffer;
			m_ui8NextTileId = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 LSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadLsb_0_Work() {
			m_ui8OamAddr = 0;
			// LSN_PPU_PATTERN_TABLES = 0x0000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +
				(static_cast<uint16_t>(m_ui8NextTileId) << 4) +
				(m_paPpuAddrV.s.ui16FineY) +
				0) );
		}

		/**
		 * The second of the 2 LSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadLsb_1_Work() {
			m_ui8OamAddr = 0;
			m_ui8NextTileLsb = m_ui8NtAtBuffer;
		}

		/**
		 * The first of the 2 MSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadMsb_0_Work() {
			m_ui8OamAddr = 0;
			// LSN_PPU_PATTERN_TABLES = 0x0000.
			m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +
				(static_cast<uint16_t>(m_ui8NextTileId) << 4) +
				(m_paPpuAddrV.s.ui16FineY) +
				8) );
		}

		/**
		 * The second of the 2 MSB read cycles.
		 */
		void LSN_FASTCALL								Pixel_GarbageLoadMsb_1_Work() {
			m_ui8OamAddr = 0;
			m_ui8NextTileMsb = m_ui8NtAtBuffer;
		}

		/**
		 * The first cycle in clearing secondary OAM.  Reads from the main OAM (which returns 0xFF during this period) and latches the value.
		 */
		void LSN_FASTCALL								Pixel_ClearOam2_0_Sprite() {
			m_ui8OamLatch = m_pbBus->Read( LSN_PR_OAMDATA );
		}

		/**
		 * The second cycle in clearing secondary OAM.  Copies the latched value read from OAM (0x2004) to the current index in the secondary OAM buffer.
		 */
		void LSN_FASTCALL								Pixel_ClearOam2_1_Sprite() {
			m_soSecondaryOam.ui8Bytes[m_ui8Oam2ClearIdx++] = m_ui8OamLatch;
			m_ui8Oam2ClearIdx %= sizeof( m_soSecondaryOam.ui8Bytes );
		}

		/**
		 * Handles populating the secondary OAM buffer during cycles 65-256.
		 */
		template <bool _bIsFirst, bool _bIsOdd>
		void LSN_FASTCALL								Pixel_Evaluation_Sprite() {
			if constexpr ( _bIsFirst ) {
				m_ui8SpriteN = m_ui8SpriteCount = 0;
				m_ui8SpriteM = m_ui8OamAddr;
				m_sesStage = LSN_SES_CHECK_NEXT;
				m_bSprite0IsInSecondary = false;
				/*
				// On the 2C02G and 2C02H, if the sprite address (OAMADDR, $2003) is not zero, the process of starting sprite evaluation triggers an OAM hardware refresh bug that causes the 8 bytes beginning at OAMADDR & $F8 to be copied and replace the first 8 bytes of OAM.
				if ( m_ui8OamAddr != 0 ) {
				}
				*/
			}
			if ( !Rendering() ) { return; }

			uint8_t ui8PrevOamAddr = m_ui8OamAddr;

#define LSN_INC_ADDR( BY )				{						\
		m_ui8OamAddr += BY;										\
		if ( m_ui8OamAddr < ui8PrevOamAddr ) {					\
			m_sesStage = LSN_SES_FINISHED_OAM_LIST;				\
			break;												\
		}														\
	}
			
			if constexpr ( _bIsOdd ) {
				// Odd cycles allow reading.
				switch ( m_sesStage ) {
					case LSN_SES_FINISHED_OAM_LIST : {}
					case LSN_SES_CHECK_NEXT : {
						/** On odd cycles, data is read from (primary) OAM **/
						m_ui8OamLatch = m_oOam.ui8Bytes[m_ui8OamAddr];	// Sprite's Y.
						//LSN_INC_ADDR( 4 );
						break;
					}
					case LSN_SES_OF_SEARCH_ADD_SPRITE : {}
					case LSN_SES_ADD_SPRITE : {
						m_ui8OamLatch = m_oOam.ui8Bytes[m_ui8OamAddr];	// Sprite's Y.
						break;
					}
				}
			}
			else {
#ifdef LSN_TEMPLATE_PPU
				int16_t i16ScanLine = int16_t( m_stCurCycle / _tDotWidth );
#else
				int16_t i16ScanLine = int16_t( m_ui16Scanline );
#endif	// LSN_TEMPLATE_PPU
				// Even cycles allow writing.
				uint8_t ui8M = (m_ui8OamAddr - m_ui8SpriteM) & 0x3;
				switch ( m_sesStage ) {
					case LSN_SES_CHECK_NEXT : {
						/** On even cycles, data is written to secondary OAM (unless secondary OAM is full, in which case it will read the value in secondary OAM instead) */
						if ( m_ui8SpriteCount < 8 ) {
							/** 1. Starting at n = 0, read a sprite's Y-coordinate (OAM[n][0], copying it to the next open slot in secondary OAM (unless 8 sprites have been found, in which case the write is ignored). */
							m_soSecondaryOam.ui8Bytes[m_ui8SpriteCount*4+ui8M] = m_ui8OamLatch;
							/** 1a. If Y-coordinate is in range, copy remaining bytes of sprite data (OAM[n][1] thru OAM[n][3]) into secondary OAM. */
							int16_t i16Diff = i16ScanLine - int16_t( m_ui8OamLatch );

							if ( i16Diff >= 0 && i16Diff < (m_pcPpuCtrl.s.ui8SpriteSize ? 16 : 8) ) {
								// Move to the copy stage.
								if ( m_ui8OamAddr == 0 ) {
									m_bSprite0IsInSecondary = true;
								}
								m_sesStage = LSN_SES_ADD_SPRITE;
								LSN_INC_ADDR( 1 );
								break;
							}
							else {
								// Sprite is of no interest.
								LSN_INC_ADDR( 4 );
							}
						}
						else {
							// A read of secondary OAM is prescribed here, but it would have 0 side-effects, so skipping.
							// Check for overflow.
							int16_t i16Diff = i16ScanLine - int16_t( m_ui8OamLatch );

							if ( i16Diff >= 0 && i16Diff < (m_pcPpuCtrl.s.ui8SpriteSize ? 16 : 8) ) {
								// Overflow, baby!
								m_psPpuStatus.s.ui8SpriteOverflow = true;
								LSN_INC_ADDR( 1 );
							}
							else {
								m_sesStage = LSN_SES_OF_SEARCH_ADD_SPRITE;
								// Let's cause a sprite-overflow bug by modifying M here!
								LSN_INC_ADDR( 1 );
								// This causes an extra 4 reads.  m_ui8SpriteM is never used from LSN_SES_OF_SEARCH_ADD_SPRITE on except to track how many bytes copied.
								m_ui8SpriteM = m_ui8OamAddr;
							}
						}
						break;
					}
					case LSN_SES_ADD_SPRITE : {
						m_soSecondaryOam.ui8Bytes[m_ui8SpriteCount*4+ui8M] = m_ui8OamLatch;
						if ( ui8M == 3 ) {
							// Copied 4 bytes.
							++m_ui8SpriteCount;
							m_sesStage = LSN_SES_CHECK_NEXT;
							LSN_INC_ADDR( 1 );
							break;
						}
						LSN_INC_ADDR( 1 );
						break;
					}
					case LSN_SES_OF_SEARCH_ADD_SPRITE : {
						if ( ui8M == 3 ) {
							LSN_INC_ADDR( 1 );
							m_sesStage = LSN_SES_FINISHED_OAM_LIST;
							break;
						}
						LSN_INC_ADDR( 1 );
						break;
					}
					case LSN_SES_FINISHED_OAM_LIST : {
						/** 4. Attempt (and fail) to copy OAM[n][0] into the next free slot in secondary OAM, and increment n (repeat until HBLANK is reached) */
						break;
					}
				}
			}

#undef LSN_INC_ADDR
		}

		/**
		 * Handles fetching sprites (copying from secondary OAM to the current-line shifters during cycles 257-320.
		 */
		template <unsigned _uSpriteIdx, unsigned _uStage>
		inline void LSN_FASTCALL						Pixel_Fetch_Sprite() {
			// 1-4: Read the Y-coordinate, tile number, attributes, and X-coordinate of the selected sprite from secondary OAM
			// ========================
			// Garbage NT fetch 0.
			// ========================
			if constexpr ( _uStage == 0 ) {
				if constexpr ( _uSpriteIdx == 0 ) {
					m_ui8ThisLineSpriteCount = m_ui8SpriteCount;
					m_ui8SpriteCount = 0;
					m_bSprite0IsInSecondaryThisLine = m_bSprite0IsInSecondary;
				}
				m_ui8SpriteN = m_soSecondaryOam.s[_uSpriteIdx].ui8Y;
			}
			if constexpr ( _uStage == 1 ) {
				m_ui8SpriteM = m_soSecondaryOam.s[_uSpriteIdx].ui8Id;
			}

			// ========================
			// Garbage NT fetch 1.
			// ========================
			if constexpr ( _uStage == 2 ) {
				m_ui8SpriteAttrib = m_soSecondaryOam.s[_uSpriteIdx].ui8Flags;
			}
			
			if constexpr ( _uStage == 3 ) {
				m_ui8SpriteX = m_soSecondaryOam.s[_uSpriteIdx].ui8X;
			}


			// ========================
			// Sprite LSB.
			// ========================
			// Pretty famous bit-flipper: https://stackoverflow.com/questions/2602823/in-c-c-whats-the-simplest-way-to-reverse-the-order-of-bits-in-a-byte
			auto FlipBits = []( uint8_t _ui8Byte ) {
				_ui8Byte = ((_ui8Byte & 0xF0) >> 4) | ((_ui8Byte & 0x0F) << 4);
				_ui8Byte = ((_ui8Byte & 0xCC) >> 2) | ((_ui8Byte & 0x33) << 2);
				_ui8Byte = ((_ui8Byte & 0xAA) >> 1) | ((_ui8Byte & 0x55) << 1);
				return _ui8Byte;

			};
			// 5-8: Read the X-coordinate of the selected sprite from secondary OAM 4 times (while the PPU fetches the sprite tile data)
			if constexpr ( _uStage == 4 ) {
				m_ui16SpritePatternTmp = 0;

#ifdef LSN_TEMPLATE_PPU
				uint16_t ui16ScanLine = uint16_t( m_stCurCycle / _tDotWidth );
#else
				uint16_t ui16ScanLine = m_ui16Scanline;
#endif	// LSN_TEMPLATE_PPU
				
				if ( _uSpriteIdx < m_ui8ThisLineSpriteCount ) {
					// Calculate m_ui16SpritePatternTmp.
					if ( !m_pcPpuCtrl.s.ui8SpriteSize ) {
						// 8-by-8.
						if ( !(m_ui8SpriteAttrib & 0x80) ) {
							// No vertical flip.
							m_ui16SpritePatternTmp = (m_pcPpuCtrl.s.ui8SpriteTileSelect << 12) |
								(m_ui8SpriteM << 4) |
								((ui16ScanLine - m_ui8SpriteN) & 0x7);
						}
						else {
							// Major vertical flippage going on here.
							m_ui16SpritePatternTmp = (m_pcPpuCtrl.s.ui8SpriteTileSelect << 12) |
								(m_ui8SpriteM << 4) |
								((7 - (ui16ScanLine - m_ui8SpriteN)) & 0x7);
						}
					}
					else {
						// 8-by-16.
						uint8_t ui8PatternLine = uint8_t( ui16ScanLine - m_ui8SpriteN );
						if ( !(m_ui8SpriteAttrib & 0x80) ) {
							// No vertical flip.
							if ( ui8PatternLine < 8 ) {
								// Top half.
								m_ui16SpritePatternTmp = ((m_ui8SpriteM & 0x01) << 12) |
									((m_ui8SpriteM & 0xFE) << 4) |
									ui8PatternLine;
							}
							else {
								// Bottom half.
								m_ui16SpritePatternTmp = ((m_ui8SpriteM & 0x01) << 12) |
									(((m_ui8SpriteM & 0xFE) + 1) << 4) |
									(ui8PatternLine & 0x7);
							}
						}
						else {
							// Major vertical flippage going on here.
							if ( ui8PatternLine < 8 ) {
								// Top half (using bottom tile).
								m_ui16SpritePatternTmp = ((m_ui8SpriteM & 0x01) << 12) |
									(((m_ui8SpriteM & 0xFE) + 1) << 4) |
									(7 - ui8PatternLine);
							}
							else {
								// Bottom half (using top tile).
								m_ui16SpritePatternTmp = ((m_ui8SpriteM & 0x01) << 12) |
									((m_ui8SpriteM & 0xFE) << 4) |
									((7 - ui8PatternLine) & 0x7);
							}
						}
					}
					uint8_t ui8Bits = m_bBus.Read( m_ui16SpritePatternTmp );
					if ( m_ui8SpriteAttrib & 0x40 ) {
						ui8Bits = FlipBits( ui8Bits );
					}
					m_asActiveSprites.ui8ShiftLo[_uSpriteIdx] = ui8Bits;
				}
				else {
					m_asActiveSprites.ui8ShiftLo[_uSpriteIdx] = 0;
				}
			}
			if constexpr ( _uStage == 5 ) {
				m_asActiveSprites.ui8Latch[_uSpriteIdx] = m_ui8SpriteAttrib;
			}


			// ========================
			// Sprite MSB.
			// ========================
			if constexpr ( _uStage == 6 ) {
				if ( _uSpriteIdx < m_ui8ThisLineSpriteCount ) {
					uint8_t ui8Bits = m_bBus.Read( (m_ui16SpritePatternTmp + 8) );
					if ( m_ui8SpriteAttrib & 0x40 ) {
						ui8Bits = FlipBits( ui8Bits );
					}
					m_asActiveSprites.ui8ShiftHi[_uSpriteIdx] = ui8Bits;
				}
				else {
					m_asActiveSprites.ui8ShiftHi[_uSpriteIdx] = 0;
				}
			}
			if constexpr ( _uStage == 7 ) {
				m_asActiveSprites.ui8X[_uSpriteIdx] = m_ui8SpriteX;
			}
		}

		/**
		 * Incrementing the X scroll register.
		 */
		void LSN_FASTCALL								Pixel_IncScrollX_Control() {
			if ( Rendering() ) {
				// m_paPpuAddrV.s.ui16CourseX is only 5 bits so wrapping is automatic with the increment.  Check if the nametable needs to be swapped before the increment.
				if ( m_paPpuAddrV.s.ui16CourseX == 31 ) {
					// m_paPpuAddrV.s.ui16NametableX is 1 bit, so just toggle.
					m_paPpuAddrV.s.ui16NametableX = !m_paPpuAddrV.s.ui16NametableX;
				}
				++m_paPpuAddrV.s.ui16CourseX;
			}
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * Incrementing the Y scroll register.  Also increments the X.
		 */
		void LSN_FASTCALL								Pixel_IncScrollY_Control() {
			if ( Rendering() ) {
				if ( m_paPpuAddrV.s.ui16FineY < 7 ) {
					++m_paPpuAddrV.s.ui16FineY;
				}
				else {
					m_paPpuAddrV.s.ui16FineY = 0;
					// Wrap and increment the course.
					// Do we need to swap vertical nametable targets?
					switch ( m_paPpuAddrV.s.ui16CourseY ) {
						case 29 : {
							// Wrap the course offset and flip the nametable bit.
							m_paPpuAddrV.s.ui16CourseY = 0;
							m_paPpuAddrV.s.ui16NametableY = ~m_paPpuAddrV.s.ui16NametableY;
							break;
						}
						case 31 : {
							// We are in attribute memory.  Reset but without flipping the nametable.
							m_paPpuAddrV.s.ui16CourseY = 0;
							break;
						}
						default : {
							// Somewhere between.  Just increment.
							++m_paPpuAddrV.s.ui16CourseY;
						}
					}
				}
			}
			Pixel_IncScrollX_Control();
		}

		/**
		 * Transfer the X values from the T pointer to the V pointer.
		 */
		void LSN_FASTCALL								Pixel_TransferX_Control() {
			if ( Rendering() ) {
				m_paPpuAddrV.s.ui16NametableX = m_paPpuAddrT.s.ui16NametableX;
				m_paPpuAddrV.s.ui16CourseX = m_paPpuAddrT.s.ui16CourseX;
			}
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * Transfer the Y values from the T pointer to the V pointer.
		 */
		void LSN_FASTCALL								Pixel_TransferY_Control() {
			if ( Rendering() ) {
				m_paPpuAddrV.s.ui16FineY = m_paPpuAddrT.s.ui16FineY;
				m_paPpuAddrV.s.ui16NametableY = m_paPpuAddrT.s.ui16NametableY;
				m_paPpuAddrV.s.ui16CourseY = m_paPpuAddrT.s.ui16CourseY;
			}
			LSN_END_CONTROL_CYCLE;
		}

		/**
		 * Determines if any rendering is taking place.
		 *
		 * \return Returns true if either the background or sprites are enabled, false otherwise.
		 */
		inline bool										Rendering() const { return m_pmPpuMask.s.ui8ShowBackground || m_pmPpuMask.s.ui8ShowSprites; }

		/**
		 * Writing to 0x2000 (PPUCTRL).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2000( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = ppPpu->m_pcPpuCtrl.ui8Reg = _ui8Val;
			ppPpu->m_paPpuAddrT.s.ui16NametableX = LSN_CTRL_NAMETABLE_X( ppPpu->m_pcPpuCtrl );
			ppPpu->m_paPpuAddrT.s.ui16NametableY = LSN_CTRL_NAMETABLE_Y( ppPpu->m_pcPpuCtrl );
		}

		/**
		 * Writing to 0x2001 (PPUMASK).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2001( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = ppPpu->m_pmPpuMask.ui8Reg = _ui8Val;
		}

		/**
		 * Reading from 0x2002 (PPUMASK) (resets the v-blank flag).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
		 * \param _pui8Data The buffer from which to read.
		 * \param _ui8Ret The read value.
		 */
		static void LSN_FASTCALL						Read2002( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			// Most registers return the I/O float and then update the I/O float with the read value.
			// 0x2002 immediately updates the floating bus.
			ppPpu->m_ui8IoBusLatch = (ppPpu->m_ui8IoBusLatch & 0x1F) | (ppPpu->m_psPpuStatus.ui8Reg & 0xE0);
			_ui8Ret = ppPpu->m_ui8IoBusLatch;
			// Reads cause the v-blank flag to reset.
			ppPpu->m_psPpuStatus.s.ui8VBlank = 0;
			// The address latch also gets reset.
			ppPpu->m_bAddresLatch = false;
		}

		/**
		 * Writing to 0x2002 (PPUSTATUS).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2002( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = _ui8Val;
		}

		/**
		 * Writing to 0x2003 (OAMADDR).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2003( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = ppPpu->m_ui8OamAddr = _ui8Val;
		}

		/**
		 * Reading from 0x2004 (OAMDATA).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
		 * \param _pui8Data The buffer from which to read.
		 * \param _ui8Ret The read value.
		 */
		static void LSN_FASTCALL						Read2004( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
#ifdef LSN_TEMPLATE_PPU
			uint16_t ui16Scan = uint16_t( ppPpu->m_stCurCycle / _tDotWidth );
			uint16_t ui16Dot = ppPpu->m_stCurCycle % _tDotWidth;
#else
			uint16_t ui16Scan = ppPpu->m_ui16Scanline;
			uint16_t ui16Dot = ppPpu->m_ui16RowDot;
#endif
			//m_stCurCycle
			if ( (ui16Scan < (_tPreRender + _tRender)) || ui16Scan == (_tDotHeight - 1) ) {
				if ( ui16Dot >= 1 && ui16Dot <= 64 ) {
					_ui8Ret = 0xFF;
					return;
				} 
			}
			/*_ui8Ret = ppPpu->m_ui8IoBusLatch;
			ppPpu->m_ui8IoBusLatch = ppPpu->m_oOam.ui8Bytes[ppPpu->m_ui8OamAddr];*/
			ppPpu->m_ui8IoBusLatch = ppPpu->m_oOam.ui8Bytes[ppPpu->m_ui8OamAddr];
			_ui8Ret = ppPpu->m_ui8IoBusLatch;
		}

		/**
		 * Writing to 0x2004 (OAMDATA).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2004( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = ppPpu->m_oOam.ui8Bytes[ppPpu->m_ui8OamAddr++] = _ui8Val;
		}

		/**
		 * Writing to 0x2005 (PPUSCROLL).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2005( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = _ui8Val;
			if ( !ppPpu->m_bAddresLatch ) {
				ppPpu->m_ui8FineScrollX = _ui8Val & 0x7;
				ppPpu->m_paPpuAddrT.s.ui16CourseX = _ui8Val >> 3;
			}
			else {
				ppPpu->m_paPpuAddrT.s.ui16FineY = _ui8Val & 0x7;
				ppPpu->m_paPpuAddrT.s.ui16CourseY = _ui8Val >> 3;
			}
			ppPpu->m_bAddresLatch ^= 1;
		}

		/**
		 * Writing to 0x2006 (PPUADDR).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2006( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			// Write top 8 bits first.  Easily acheived by flipping the latch before writing.
			ppPpu->m_ui8IoBusLatch = _ui8Val;
			ppPpu->m_bAddresLatch ^= 1;
			ppPpu->m_paPpuAddrT.ui8Bytes[ppPpu->m_bAddresLatch] = _ui8Val;
			ppPpu->m_paPpuAddrT.ui16Addr &= (ppPpu->m_bBus.Size() - 1);
			if ( !ppPpu->m_bAddresLatch ) {
				// ppPpu->m_bAddresLatch was 1 when we came here, flipped at the start.  This is the 2nd write.
				ppPpu->m_paPpuAddrV.ui16Addr = ppPpu->m_paPpuAddrT.ui16Addr;
			}
		}

		/**
		 * Reading from 0x2007 (PPUDATA) (PPU bus memory/PPUDATA).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
		 * \param _pui8Data The buffer from which to read.
		 * \param _ui8Ret The read value.
		 */
		static void LSN_FASTCALL						Read2007( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			uint16_t ui16Addr = ppPpu->m_paPpuAddrV.ui16Addr & (LSN_PPU_MEM_FULL_SIZE - 1);
			if ( ui16Addr >= LSN_PPU_PALETTE_MEMORY ) {
				// Palette memory is placed on the bus and returned immediately.
				_ui8Ret = ppPpu->m_ui8IoBusLatch = ppPpu->m_bBus.Read( ui16Addr );
				ppPpu->m_ui8DataBuffer = ppPpu->m_bBus.Read( (ui16Addr & 0x7FF) | 0x2000 );
				//ppPpu->m_ui8DataBuffer = _pui8Data[(ui16Addr & 0x7FF) | 0x2000];
			}
			else {
				// For every other address the floating-bus contents are returned and the floater is updated with the requested value
				//	to be fetched on the next read.
				_ui8Ret = ppPpu->m_ui8IoBusLatch = ppPpu->m_ui8DataBuffer;
				ppPpu->m_ui8DataBuffer = ppPpu->m_bBus.Read( ui16Addr );
			}
			ppPpu->m_paPpuAddrV.ui16Addr = (ui16Addr + (ppPpu->m_pcPpuCtrl.s.ui8IncrementMode ? 32 : 1)) & (LSN_PPU_MEM_FULL_SIZE - 1);
		}

		/**
		 * Writing to 0x2007 (PPUDATA) (PPU bus memory/PPUDATA).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						Write2007( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			uint16_t ui16Addr = ppPpu->m_paPpuAddrV.ui16Addr & (LSN_PPU_MEM_FULL_SIZE - 1);
			ppPpu->m_bBus.Write( ui16Addr, _ui8Val );
			ppPpu->m_ui8IoBusLatch = _ui8Val;
			ppPpu->m_paPpuAddrV.ui16Addr = (ui16Addr + (ppPpu->m_pcPpuCtrl.s.ui8IncrementMode ? 32 : 1)) & (LSN_PPU_MEM_FULL_SIZE - 1);
		}

		/**
		 * Reading from the palette
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
		 * \param _pui8Data The buffer from which to read.
		 * \param _ui8Ret The read value.
		 */
		static void LSN_FASTCALL						PaletteRead( void * /*_pvParm0*/, uint16_t _ui16Parm1, uint8_t * _pui8Data, uint8_t &_ui8Ret ) {
			//CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			//_ui8Ret = ppPpu->m_ui8IoBusLatch;

			//ppPpu->m_ui8IoBusLatch = _pui8Data[_ui16Parm1];
			_ui8Ret = /*ppPpu->m_ui8IoBusLatch = ppPpu->m_ui8DataBuffer =*/ _pui8Data[_ui16Parm1];
		}

		/**
		 * Writing to the background index of the palettes ($3F04/$3F08/$3F0C/$3F10/$3F14/$3F18/$3F1C).  These address can contain the written data but also update $3F00.
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						WritePaletteIdx4( void * /*_pvParm0*/, uint16_t _ui16Parm1, uint8_t * _pui8Data, uint8_t _ui8Val ) {
			_pui8Data[_ui16Parm1] /*= _pui8Data[LSN_PPU_PALETTE_MEMORY]*/ = _ui8Val;
		}

		/**
		 * Reading from a write-only address (returns the IO bus contents and fills the bus with the contents at the address).
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
		 * \param _pui8Data The buffer from which to read.
		 * \param _ui8Ret The read value.
		 */
		static void LSN_FASTCALL						PpuNoRead( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			_ui8Ret = ppPpu->m_ui8IoBusLatch;

			//ppPpu->m_ui8IoBusLatch = _pui8Data[_ui16Parm1];
		}

		/**
		 * A function usable for addresses that can't be written on the PPU.
		 *
		 * \param _pvParm0 A data value assigned to this address.
		 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
		 * \param _pui8Data The buffer to which to write.
		 * \param _ui8Ret The value to write.
		 */
		static void LSN_FASTCALL						PpuNoWrite( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
			CPpu2C0X * ppPpu = reinterpret_cast<CPpu2C0X *>(_pvParm0);
			ppPpu->m_ui8IoBusLatch = _ui8Val;
		}


	protected :
		// == Types.
		/** Function pointer for per-cycle work. */
		typedef void (LSN_FASTCALL CPpu2C0X:: *			PfCycles)();

		/** The PPUCTRL register. */
		struct LSN_PPUCTRL {
			union {
				struct {
					uint8_t								ui8Nametable			: 2;					/**< Base nametable address. */
					uint8_t								ui8IncrementMode		: 1;					/**< VRAM address-increment per CPU read/write of PPUDATA. */
					uint8_t								ui8SpriteTileSelect		: 1;					/**< Sprite pattern table address for 8-by-8 tiles. */
					uint8_t								ui8BackgroundTileSelect	: 1;					/**< Background pattern table. */
					uint8_t								ui8SpriteSize			: 1;					/**< Sprite size (0 = 8-by-8, 1 = 8-by-16). */
					uint8_t								ui8Slave				: 1;					/**< Master/slave setting. */
					uint8_t								ui8Nmi					: 1;					/**< Generate an NMI at the start of v-blank (0 = no, 1 = yes). */
				}										s;

				uint8_t									ui8Reg;											/**< Register as a uint8_t. */
			};
		};

		/** The PPUMASK register. */
		struct LSN_PPUMASK {
			union {
				struct {
					uint8_t								ui8Greyscale			: 1;					/**< 0 = color, 1 = greyscale. */
					uint8_t								ui8LeftBackground		: 1;					/**< Show the background in the left-most 8 pixels. */
					uint8_t								ui8LeftSprites			: 1;					/**< Show sprites in the left-most 8 pixels. */
					uint8_t								ui8ShowBackground		: 1;					/**< Show the background. */
					uint8_t								ui8ShowSprites			: 1;					/**< Show sprites. */
					uint8_t								ui8RedEmph				: 1;					/**< Emphasize red (green on PAL/Dendy). */
					uint8_t								ui8GreenEmph			: 1;					/**< Emphasize green (red on PAL/Dendy). */
					uint8_t								ui8BlueEmph				: 1;					/**< Emphasize blue. */
				}										s;

				uint8_t									ui8Reg;											/**< Register as a uint8_t. */
			};
		};

		/** The PPUSTATUS register. */
		struct LSN_PPUSTATUS {
			union {
				struct {
					uint8_t								ui8OpenBus				: 5;					/**< Unused bits redirect to the open bus. */
					uint8_t								ui8SpriteOverflow		: 1;					/**< Sprite-overflow flag. */
					uint8_t								ui8Sprite0Hit			: 1;					/**< Sprite-0 hit flag. */
					uint8_t								ui8VBlank				: 1;					/**< V-blank flag. */
				}										s;

				uint8_t									ui8Reg;											/**< Register as a uint8_t. */
			};
		};

		/** The PPUADDR register (loopy). */
		struct LSN_PPUADDR {
			union {
				uint16_t								ui16Addr;										/**< The full 16-bit address. */
				uint8_t									ui8Bytes[2];									/**< Per-byte access to the address. */
				struct {
					uint16_t							ui16CourseX				: 5;					/**< Course X scroll position. */
					uint16_t							ui16CourseY				: 5;					/**< Course Y scroll position. */
					uint16_t							ui16NametableX			: 1;					/**< Nametable X. */
					uint16_t							ui16NametableY			: 1;					/**< Nametable Y. */
					uint16_t							ui16FineY				: 3;					/**< Fine Y. */
				}										s;
			};
		};

		/** Cycle handlers (function pointers doing per-cycle/per-pixel work). */
		struct LSN_CYCLE {
			PfCycles									pfFunc;											/**< The function pointer doing the work on this pixel. */
		};

		/** OAM memory. */
		struct LSN_OAM {
			union {
				struct {
					uint8_t								ui8Y;											/**< Sprite Y position. */
					uint8_t								ui8Id;											/**< Pattern-memory tile ID. */
					uint8_t								ui8Flags;										/**< Flags indicating how to render the sprite. */
					uint8_t								ui8X;											/**< Sprite X position. */
				}										s[64];
				uint32_t								ui32Obj[64];
				uint8_t									ui8Bytes[256];
			};
		};

		/** Secondary OAM memory. */
		struct LSN_SECONDARY_OAM {
			union {
				struct {
					uint8_t								ui8Y;											/**< Sprite Y position. */
					uint8_t								ui8Id;											/**< Pattern-memory tile ID. */
					uint8_t								ui8Flags;										/**< Flags indicating how to render the sprite. */
					uint8_t								ui8X;											/**< Sprite X position. */
				}										s[8];
				uint32_t								ui32Obj[8];
				uint8_t									ui8Bytes[32];
			};
		};

		/** Sprites loaded to be rendered on the current scanline. */
		struct LSN_ACTIVE_SPRITE {
			uint8_t										ui8ShiftLo[8];									/**< The first of a pair of shift registers for sprite attributes/patterns. */
			uint8_t										ui8ShiftHi[8];									/**< The second of a pair of shift registers for sprite attributes/patterns. */
			uint8_t										ui8Latch[8];									/**< Attribute bytes for the sprite. */
			uint8_t										ui8X[8];										/**< The X position of the sprite. */
		};


		// == Members.
		LSN_PALETTE										m_pPalette;
		uint64_t										m_ui64Frame;									/**< The frame counter. */
		uint64_t										m_ui64Cycle;									/**< The cycle counter. */
		LSN_ACTIVE_SPRITE								m_asActiveSprites;								/**< The active sprites. */
		CCpuBus *										m_pbBus;										/**< Pointer to the bus. */
		CNmiable *										m_pnNmiTarget;									/**< The target object of NMI notifications. */
#ifdef LSN_TEMPLATE_PPU
		PfCycles										m_cCycle[_tDotWidth*_tDotHeight];				/**< The cycle function. */
		size_t											m_stCurCycle;									/**< The current cycle function. */
#endif	// #ifdef LSN_TEMPLATE_PPU
		LSN_CYCLE										m_cControlCycles[_tDotWidth*_tDotHeight];		/**< The per-pixel array of function pointers to do per-cycle control work.  Control work relates to setting flags and maintaining the register state, etc. */
		LSN_CYCLE										m_cWorkCycles[_tDotWidth*_tDotHeight];			/**< The per-pixel array of function pointers to do per-cycle rendering work.  Standard work cycles are used to fetch data and render the results. */
		LSN_CYCLE										m_cSpriteCycles[_tDotWidth*_tDotHeight];		/**< The per-pixel array of function pointers to do per-cycle sprite-handing work.  Sprite work cycles are used to fetch sprite data for rendering. */
		CPpuBus											m_bBus;											/**< The PPU's internal RAM. */
		LSN_OAM											m_oOam;											/**< OAM memory. */
		LSN_SECONDARY_OAM								m_soSecondaryOam;								/**< Secondary OAM used for rendering during a scanline. */
		LSN_PPUADDR										m_paPpuAddrT;									/**< The "t" PPUADDR register. */
		LSN_PPUADDR										m_paPpuAddrV;									/**< The "v" PPUADDR register. */
		LSN_PPUCTRL										m_pcPpuCtrl;									/**< The PPUCTRL register. */
		LSN_PPUMASK										m_pmPpuMask;									/**< The PPUMASK register. */
		LSN_PPUSTATUS									m_psPpuStatus;									/**< The PPUSTATUS register. */
		LSN_SPRITE_EVAL_STATE							m_sesStage;										/**< The sprite-evaluation stage. */
		uint16_t										m_ui16Scanline;									/**< The scanline counter. */
		uint16_t										m_ui16RowDot;									/**< The horizontal counter. */
		uint16_t										m_ui16ShiftPatternLo;							/**< The 16-bit shifter for the pattern low bits. */
		uint16_t										m_ui16ShiftPatternHi;							/**< The 16-bit shifter for the pattern high bits. */
		uint16_t										m_ui16ShiftAttribLo;							/**< The 16-bit shifter for the attribute low bits. */
		uint16_t										m_ui16ShiftAttribHi;							/**< The 16-bit shifter for the attribute high bits. */
		uint16_t										m_ui16SpritePatternTmp;							/**< A temporary used during sprite fetches. */
		uint8_t											m_ui8IoBusLatch;								/**< The I/O bus floater. */
		uint8_t											m_ui8DataBuffer;								/**< The $2007 (PPUDATA) buffer. */
		uint8_t											m_ui8FineScrollX;								/**< The fine X scroll position. */
		uint8_t											m_ui8NtAtBuffer;								/**< I guess the 2 cycles of the NT/AT load first store the value into a temprary and then into the latch (to later be masked out every 8th cycle)? */
		uint8_t											m_ui8OamAddr;									/**< OAM address. */
		uint8_t											m_ui8OamLatch;									/**< Holds temporary OAM data. */
		uint8_t											m_ui8Oam2ClearIdx;								/**< The index of the byte being cleared during the secondary OAM clear. */

		uint8_t											m_ui8SpriteN;									/**< The N index during sprite evaluation and the sprite Y during fetches. */
		uint8_t											m_ui8SpriteM;									/**< The M index during sprite evaluation and the sprite tile number during fetches. */
		uint8_t											m_ui8SpriteAttrib;								/**< The sprite attributes during fetches. */
		uint8_t											m_ui8SpriteX;									/**< The sprite X during fetches. */
		uint8_t											m_ui8SpriteCount;								/**< The number of sprites transferred to the secondary OAM array. */

		uint8_t											m_ui8NextTileId;								/**< The queued background tile ID during rendering. */
		uint8_t											m_ui8NextTileAttribute;							/**< The queued background tile attribute during rendering. */
		uint8_t											m_ui8NextTileLsb;								/**< The queued background tile LSB. */
		uint8_t											m_ui8NextTileMsb;								/**< The queued background tile MSB. */
		uint8_t											m_ui8ThisLineSpriteCount;						/**< The number of sprites in the current scanline. */

		bool											m_bAddresLatch;									/**< The address latch. */
		bool											m_bSprite0IsInSecondary;						/**< Set during sprite evaluation, this indicates that the first sprite in secondary OAM is sprite 0. */
		bool											m_bSprite0IsInSecondaryThisLine;				/**< Copied to m_bSprite0IsInSecondary during sprite fetching, used to determine if sprite 0 is in the current line being drawn. */

		// TMP.
		struct LSN_DEBUG_COPY {
			uint64_t									m_ui64Frame;									/**< The frame counter. */
			LSN_PPUADDR									m_paPpuAddrT;									/**< The "t" PPUADDR register. */
			LSN_PPUADDR									m_paPpuAddrV;									/**< The "v" PPUADDR register. */
			LSN_PPUSTATUS								m_psPpuStatus;									/**< The PPUSTATUS register. */
			uint16_t									m_ui16Scanline;									/**< The scanline counter. */
			uint16_t									m_ui16RowDot;									/**< The horizontal counter. */

			uint16_t									m_ui16ShiftPatternLo;							/**< The 16-bit shifter for the pattern low bits. */
			uint16_t									m_ui16ShiftPatternHi;							/**< The 16-bit shifter for the pattern high bits. */
			uint16_t									m_ui16ShiftAttribLo;							/**< The 16-bit shifter for the attribute low bits. */
			uint16_t									m_ui16ShiftAttribHi;							/**< The 16-bit shifter for the attribute high bits. */

			uint8_t										m_ui8FineScrollX;								/**< The fine X scroll position. */
			uint8_t										m_ui8NtAtBuffer;								/**< I guess the 2 cycles of the NT/AT load first store the value into a temprary and then into the latch (to later be masked out every 8th cycle)? */

			uint8_t										m_ui8NextTileId;								/**< The queued background tile ID during rendering. */
			uint8_t										m_ui8NextTileAttribute;							/**< The queued background tile attribute during rendering. */
			uint8_t										m_ui8NextTileLsb;								/**< The queued background tile LSB. */
			uint8_t										m_ui8NextTileMsb;								/**< The queued background tile MSB. */
		};


		// == Functions.
		/**
		 * Assigns the gather/render functions at a given X/Y pixel location.
		 *
		 * \param _stX The X pixel location from which to begin assiging the gather/render functions.
		 * \param _stY The Y pixel location from which to begin assiging the gather/render functions.
		 * \param _bIncludeControls If true, control functions such as incrementing the horizontal and vertical addresses are added.
		 * \param _bDummy If true, functions to use the latched values are not added.
		 */
		void											AssignGatherRenderFuncs( size_t _stX, size_t _stY, bool _bIncludeControls, bool /*_bDummy*/ ) {
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadNt_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadNt_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadAt_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadAt_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadLsb_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadLsb_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadMsb_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_LoadMsb_1_Work;
			}
			if ( _bIncludeControls ) {
				_stX -= 8;
				{
					LSN_CYCLE & cThis = m_cControlCycles[_stY*_tDotWidth+(_stX+7)];
					cThis.pfFunc = &CPpu2C0X::Pixel_IncScrollX_Control;
				}
			}
		}

		/**
		 * Assigns the garbage gather/render functions at a given X/Y pixel location.
		 *
		 * \param _stX The X pixel location from which to begin assiging the gather/render functions.
		 * \param _stY The Y pixel location from which to begin assiging the gather/render functions.
		 */
		void											AssignGarbageGatherRenderFuncs( size_t _stX, size_t _stY ) {
			//AssignGatherRenderFuncs( _stX, _stY, false, false );
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadNt_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadNt_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadAt_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadAt_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadLsb_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadLsb_1_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadMsb_0_Work;
			}
			{
				LSN_CYCLE & cThis = m_cWorkCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_GarbageLoadMsb_1_Work;
			}
		}

		/**
		 * Assigns the sprite-fetch functions (for a given sprite index) at a given X/Y pixel location.
		 *
		 * \param _stX The X pixel location from which to begin assiging the fetch functions.
		 * \param _stY The Y pixel location from which to begin assiging the fetch functions.
		 */
		template <unsigned _uSpriteIdx>
		inline void										AssignSpriteFetchFuncs_BySpriteIdx( size_t _stX, size_t _stY ) {
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 0>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 1>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 2>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 3>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 4>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 5>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX++];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 6>;
			}
			{
				LSN_CYCLE & cThis = m_cSpriteCycles[_stY*_tDotWidth+_stX];
				cThis.pfFunc = &CPpu2C0X::Pixel_Fetch_Sprite<_uSpriteIdx, 7>;
			}
		}

		/**
		 * Assigns the sprite-fetch functions at a given X/Y pixel location.
		 *
		 * \param _stX The X pixel location from which to begin assiging the fetch functions.
		 * \param _stY The Y pixel location from which to begin assiging the fetch functions.
		 */
		void											AssignSpriteFetchFuncs( size_t _stX, size_t _stY ) {
			AssignSpriteFetchFuncs_BySpriteIdx<0>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<1>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<2>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<3>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<4>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<5>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<6>( _stX, _stY );
			_stX += 8;
			AssignSpriteFetchFuncs_BySpriteIdx<7>( _stX, _stY );
		}

		/**
		 * Applies the function pointers for a given render scanline.
		 *
		 * \param _ui16Scanline The scanline whose function pointers for standard rendering are to be set.
		 */
		void											ApplyStdRenderFunctionPointers( uint16_t _ui16Scanline ) {
#define LSN_LEFT				1
#define LSN_RIGHT				(_tRenderW + LSN_LEFT)
#define LSN_SPR_RIGHT			(LSN_RIGHT + 8 * 8)
			const auto Y = _ui16Scanline;
			for ( auto X = LSN_LEFT; X < LSN_RIGHT; X += 8 ) {
				AssignGatherRenderFuncs( X, Y, true, false );
			}
			// Garbage reads.
			for ( auto X = LSN_RIGHT; X < LSN_SPR_RIGHT; X += 8 ) {
				AssignGarbageGatherRenderFuncs( X, Y );
			}
			// Dummy reads.
			for ( auto X = LSN_SPR_RIGHT; X < (_tDotWidth - 4); X += 8 ) {
				AssignGatherRenderFuncs( X, Y, false, true );
			}
			{	// Unused NT fetches.
				{
					LSN_CYCLE & cThis = m_cWorkCycles[Y*_tDotWidth+(_tDotWidth-4)];
					cThis.pfFunc = &CPpu2C0X::Pixel_LoadNtNoShift_0_Work;
				}
				{
					LSN_CYCLE & cThis = m_cWorkCycles[Y*_tDotWidth+(_tDotWidth-3)];
					cThis.pfFunc = &CPpu2C0X::Pixel_LoadNtNoShift_1_Work;
				}
				{
					LSN_CYCLE & cThis = m_cWorkCycles[Y*_tDotWidth+(_tDotWidth-2)];
					cThis.pfFunc = &CPpu2C0X::Pixel_LoadNtNoShift_0_Work;
				}
				{
					LSN_CYCLE & cThis = m_cWorkCycles[Y*_tDotWidth+(_tDotWidth-1)];
					cThis.pfFunc = &CPpu2C0X::Pixel_LoadNtNoShift_1_Work;
				}
			}
			{	// Increase vertical.
				LSN_CYCLE & cThis = m_cControlCycles[Y*_tDotWidth+256];
				cThis.pfFunc = &CPpu2C0X::Pixel_IncScrollY_Control;
			}
			{	// Transfer horizontal.
				LSN_CYCLE & cThis = m_cControlCycles[Y*_tDotWidth+257];
				cThis.pfFunc = &CPpu2C0X::Pixel_TransferX_Control;
			}
			AssignGatherRenderFuncs( LSN_SPR_RIGHT, Y, true, false );
			AssignGatherRenderFuncs( LSN_SPR_RIGHT + 8, Y, true, false );

			if ( _tDotHeight - 1 == Y ) {
				for ( auto X = 280; X <= 304; ++X ) {
					{	// Transfer vertical.
						LSN_CYCLE & cThis = m_cControlCycles[Y*_tDotWidth+X];
						cThis.pfFunc = &CPpu2C0X::Pixel_TransferY_Control;
					}
				}
			}

#undef LSN_SPR_RIGHT
#undef LSN_RIGHT
#undef LSN_LEFT
		}

		/**
		 * Applies the sprite-shepherding function pointers for a given render scanline.
		 *
		 * \param _ui16Scanline The scanline whose function pointers for sprite-handling are to be set.
		 */
		void											ApplySpriteFunctionPointers( uint16_t _ui16Scanline ) {
#define LSN_LEFT				1
#define LSN_EVAL_START			(LSN_LEFT + 8 * 8)
#define LSN_FETCH_START			(LSN_EVAL_START + 8 * 24)
			const auto Y = _ui16Scanline;
			// Clear secondary OAM.
			for ( auto X = LSN_LEFT; X < LSN_EVAL_START; X += 2 ) {
				{	// Latch data.
					LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+(X+0)];
					cThis.pfFunc = &CPpu2C0X::Pixel_ClearOam2_0_Sprite;
				}
				{	// Write data.
					LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+(X+1)];
					cThis.pfFunc = &CPpu2C0X::Pixel_ClearOam2_1_Sprite;
				}
			}
			size_t stX = LSN_EVAL_START;
			{	// Start evaluation process.
				LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+LSN_EVAL_START];
				cThis.pfFunc = &CPpu2C0X::Pixel_Evaluation_Sprite<true, true>;
			}
			{	// Start evaluation process.
				LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+(stX+1)];
				cThis.pfFunc = &CPpu2C0X::Pixel_Evaluation_Sprite<false, false>;
			}
			stX += 2;
			// Finish the evaluation process.
			for ( ; stX < LSN_FETCH_START; stX += 2 ) {
				{	// Start evaluation process.
					LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+stX];
					cThis.pfFunc = &CPpu2C0X::Pixel_Evaluation_Sprite<false, true>;
				}
				{	// Start evaluation process.
					LSN_CYCLE & cThis = m_cSpriteCycles[Y*_tDotWidth+(stX+1)];
					cThis.pfFunc = &CPpu2C0X::Pixel_Evaluation_Sprite<false, false>;
				}
			}
			AssignSpriteFetchFuncs( LSN_FETCH_START, Y );

#undef LSN_FETCH_START
#undef LSN_EVAL_START
#undef LSN_LEFT
		}

		/**
		 * Shifts the background registers left one.
		 */
		inline void										ShiftBackgroundAndSpriteRegisters() {
			if ( Rendering() ) {
				m_ui16ShiftPatternLo <<= 1;
				m_ui16ShiftPatternHi <<= 1;
				m_ui16ShiftAttribLo <<= 1;
				m_ui16ShiftAttribHi <<= 1;
				if ( m_ui16RowDot >= 2 && m_ui16RowDot <= 256 ) {
					for ( uint8_t I = m_ui8ThisLineSpriteCount; I--; ) {
						if ( m_asActiveSprites.ui8X[I] ) {
							--m_asActiveSprites.ui8X[I];
						}
						else {
							m_asActiveSprites.ui8ShiftLo[I] <<= 1;
							m_asActiveSprites.ui8ShiftHi[I] <<= 1;
						}
					}
				}
			}
		}

		/**
		 * Loads the latched tile data into the shift registers.
		 */
		inline void										LoadLatchedBackgroundIntoShiftRegisters() {
			//if ( m_ui16RowDot >= 9 && m_ui16RowDot <= 257 ) {
			if ( Rendering() ) {
				m_ui16ShiftPatternLo = (m_ui16ShiftPatternLo & 0xFF00) | m_ui8NextTileLsb;
				m_ui16ShiftPatternHi = (m_ui16ShiftPatternHi & 0xFF00) | m_ui8NextTileMsb;

				m_ui16ShiftAttribLo  = (m_ui16ShiftAttribLo & 0xFF00) | ((m_ui8NextTileAttribute & 0b01) ? 0xFF : 0x00);
				m_ui16ShiftAttribHi  = (m_ui16ShiftAttribHi & 0xFF00) | ((m_ui8NextTileAttribute & 0b10) ? 0xFF : 0x00);
			}
			//}
		}

		/**
		 * Given an X and a Y scanline/row-cycle combination, determine if the pixel is represents can be output to the render target.
		 *
		 * \param _ui16X The X (row cycle).
		 * \param _ui16Y The Y (scanline).
		 * \param _ui16RtX The resulting render-target X if true is returned.
		 * \param _ui16RtY The resulting render-target Y if true is returned.
		 * \return Returns true if the given X and Y are contained within the render target
		 */
		inline bool										CycleToRenderTarget( uint16_t _ui16X, uint16_t _ui16Y, uint16_t &_ui16RtX, uint16_t &_ui16RtY ) {
			if ( _ui16X >= 1 && _ui16X < (_tRenderW + 1) ) {
				if ( _ui16Y < (_tPreRender + _tRender) ) {
					_ui16RtX = _ui16X - 1;
					_ui16RtY = (_tPreRender + _tRender - 1) - _ui16Y;
					return true;
				}
			}
			return false;
		}

		/**
		 * Renders a pixel based on the current state of the PPU.
		 */
		inline void										RenderPixel() {
#ifdef LSN_TEMPLATE_PPU
			uint16_t ui16ThisX = uint16_t( m_stCurCycle % _tDotWidth ), ui16ThisY = uint16_t( m_stCurCycle / _tDotWidth );
#else
			uint16_t ui16ThisX = m_ui16RowDot, ui16ThisY = m_ui16Scanline;
#endif	// #ifdef LSN_TEMPLATE_PPU
			uint16_t ui16X, ui16Y;
			if ( CycleToRenderTarget( ui16ThisX, ui16ThisY, ui16X, ui16Y ) && m_pui8RenderTarget ) {
				uint8_t * pui8RenderPixel = &m_pui8RenderTarget[ui16Y*m_stRenderTargetStride+ui16X*3];
				uint8_t ui8Val = 0x0F;
				if ( ui16Y >= _tRender ) {}													// Black pre-render scanline on PAL.
				else {
					uint8_t ui8BackgroundPixel = 0;
					uint8_t ui8BackgroundPalette = 0;
					if ( m_pmPpuMask.s.ui8ShowBackground && (m_pmPpuMask.s.ui8LeftBackground || ui16X >= 8) ) {
						const uint16_t ui16Bit = 0x8000 >> m_ui8FineScrollX;
						ui8BackgroundPixel = (((m_ui16ShiftPatternHi & ui16Bit) > 0) << 1) |
							((m_ui16ShiftPatternLo & ui16Bit) > 0);
						ui8BackgroundPalette = (((m_ui16ShiftAttribHi & ui16Bit) > 0) << 1) |
							((m_ui16ShiftAttribLo & ui16Bit) > 0);
					}

					uint8_t ui8ForegroundPixel = 0;
					uint8_t ui8ForegroundPalette = 0;
					uint8_t ui8ForegroundPriority = 0;
					bool bIsRenderingSprite0 = false;
					if ( m_pmPpuMask.s.ui8ShowSprites && (m_pmPpuMask.s.ui8LeftSprites || ui16X >= 8) ) {
						for ( uint8_t I = 0; I < m_ui8ThisLineSpriteCount; ++I ) {
							if ( m_asActiveSprites.ui8X[I] == 0 ) {
								ui8ForegroundPixel = (((m_asActiveSprites.ui8ShiftHi[I] & 0x80) > 0) << 1) |
									((m_asActiveSprites.ui8ShiftLo[I] & 0x80) > 0);
								ui8ForegroundPalette = (m_asActiveSprites.ui8Latch[I] & 0x03) + 4;
								ui8ForegroundPriority = (m_asActiveSprites.ui8Latch[I] & 0x20) == 0;

								if ( ui8ForegroundPixel != 0 ) {
									bIsRenderingSprite0 = (I == 0);
									break;
								}
							}
						}
					}

					// Handle priority.
					uint8_t ui8FinalPixel = 0;
					uint8_t ui8FinalPalette = 0;

					if ( ui8BackgroundPixel && !ui8ForegroundPixel ) {
						ui8FinalPixel = ui8BackgroundPixel;
						ui8FinalPalette = ui8BackgroundPalette;
					}
					else if ( !ui8BackgroundPixel && ui8ForegroundPixel ) {
						ui8FinalPixel = ui8ForegroundPixel;
						ui8FinalPalette = ui8ForegroundPalette;
					}
					else if ( ui8BackgroundPixel && ui8ForegroundPixel ) {
						if ( ui8ForegroundPriority ) {
							ui8FinalPixel = ui8ForegroundPixel;
							ui8FinalPalette = ui8ForegroundPalette;
						}
						else {
							ui8FinalPixel = ui8BackgroundPixel;
							ui8FinalPalette = ui8BackgroundPalette;
						}


						// Since we already made the "ui8BackgroundPixel && ui8ForegroundPixel" check, handle sprite-0 here too.
						// If sprite 0 was in the secondary OAM last scanline, m_bSprite0IsInSecondaryThisLine is set for this scanline.
						if ( m_bSprite0IsInSecondaryThisLine && bIsRenderingSprite0 ) {
							// bIsRenderingSprite0 automatically means that sprites are enabled and that this pixel was not 0.
							if ( !(m_pmPpuMask.s.ui8LeftBackground | m_pmPpuMask.s.ui8LeftSprites) && !m_psPpuStatus.s.ui8Sprite0Hit && ui16X != 255 ) {
								if ( ui16X >= 8 ) {
									m_psPpuStatus.s.ui8Sprite0Hit = 1;
								}
							}
							else {
								m_psPpuStatus.s.ui8Sprite0Hit = 1;
							}
						}
					}

					ui8Val = m_bBus.Read( 0x3F00 + (ui8FinalPalette << 2) | ui8FinalPixel ) & 0x3F;
					// https://archive.nes.science/nesdev-forums/f3/t8209.xhtml#p85078
					// Mine is: none, red, green, red+green, blue, blue+red, blue+green, all.
					ui8Val |= (m_pmPpuMask.s.ui8RedEmph << 6);
					ui8Val |= (m_pmPpuMask.s.ui8GreenEmph << 7);
					ui8Val |= (m_pmPpuMask.s.ui8BlueEmph << 8);
//#define LSN_SHOW_PIXEL
#ifdef LSN_SHOW_PIXEL
					ui8Val = ui8BackgroundPalette * (255 / 4);// + ui8BackgroundPixel * 10;	// TMP
#endif	// #ifdef LSN_SHOW_PIXEL
				}
					
#ifdef LSN_SHOW_PIXEL
				pui8RenderPixel[0] = ui8Val;
				pui8RenderPixel[1] = ui8Val;
				pui8RenderPixel[2] = ui8Val;
#else

				if ( ui16X < _tBorderW || ui16X >= (_tRenderW - _tBorderW) ) {				// Horizontal black border on PAL.
					// Above pixel processing done because Sprite 0 can be hit even inside these borders.
					pui8RenderPixel[0] = 0;
					pui8RenderPixel[1] = 0;
					pui8RenderPixel[2] = 0;
				}
				else {
					pui8RenderPixel[0] = m_pPalette.uVals[ui8Val].ui8Rgb[0];
					pui8RenderPixel[1] = m_pPalette.uVals[ui8Val].ui8Rgb[1];
					pui8RenderPixel[2] = m_pPalette.uVals[ui8Val].ui8Rgb[2];
				}
#endif	// #ifdef LSN_SHOW_PIXEL
				
				//ppuRead(0x3F00 + (palette << 2) + pixel) & 0x3F
			}
		}


#ifdef LSN_TEMPLATE_PPU
		/**
		 * Returns the scanline converted to a render-target Y.
		 *
		 * \param _uScanline The scanline.
		 * \return Returns the scanline converted to a render-target Y. 
		 */
		template <unsigned _uScanline>
		static constexpr int16_t						ScanlineToRenderTarget() {
			return int16_t( _tPreRender + _tRender - 1 ) - int16_t( _uScanline );
		}

		/**
		 * Returns the row dot converted to a render-target X.
		 *
		 * \param _uDot The row dot.
		 * \return Returns the row dot converted to a render-target X.
		 */
		template <unsigned _uDot>
		static constexpr int16_t						DotToRenderTarget() {
			return int16_t( _uDot ) - 1;
		}

		/**
		 * Returns true if the given scanline and dot are within the render target.
		 *
		 * \param _uDot The row dot.
		 * \param _uScanline The scanline.
		 * \return Returns true if the [dot,scanline] conversion gives a result that is inside the render target.
		 */
		template <unsigned _uX, unsigned _uY>
		static constexpr bool							InRenderTargetView() {
			return (uint16_t( ScanlineToRenderTarget<_uY>() ) < (_tPreRender + _tRender)) &&
				(uint16_t( DotToRenderTarget<_uX>() ) < _tRenderW);
		}

#ifdef LSN_GEN_PPU
		/**
		 * Executing a single PPU cycle.
		 */
		//template <unsigned _uX, unsigned _uY>
		std::string										Cycle_Generate( uint16_t _uX, uint16_t _uY ) {
#define LSN_LEFT				1
#define LSN_SPR_EVAL_START		(LSN_LEFT + 8 * 8)			// 65.
#define LSN_RIGHT				(_tRenderW + LSN_LEFT)		// 257.
#define LSN_NEXT_TWO			(LSN_RIGHT + 8 * 8)			// 321.
#define LSN_DUMMY_BEGIN			(LSN_NEXT_TWO + 8 * 2)		// 337.

			std::string sRet;

			const uint16_t ui61RenderHeight = _tPreRender + _tRender;
			

			// Unused reads at the end of a line.
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				(_uX >= LSN_DUMMY_BEGIN) ) {
				{
					if ( (_uX - LSN_LEFT) % 2 == 0 ) {
						sRet += "\r\n"
						"// LSN_PPU_NAMETABLES = 0x2000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );\r\n";
					}
					if ( (_uX - LSN_LEFT) % 2 == 1 ) {
						sRet += "\r\n"
						"/*m_ui8NextTileId = */m_ui8NtAtBuffer;\r\n";
					}
				}
			}


			// Sprites.
			if ( (_uY >= 0 && _uY < ui61RenderHeight) ) {
				// Sprite secondary OAM clear
				if ( (_uX >= LSN_LEFT && _uX < LSN_SPR_EVAL_START) ) {
					if ( (_uX - LSN_LEFT) % 2 == 0 ) {
						sRet += "\r\n"
						"m_ui8OamLatch = m_pbBus->Read( LSN_PR_OAMDATA );\r\n";
					}
					if ( (_uX - LSN_LEFT) % 2 == 1 ) {
						sRet += "\r\n"
						"m_soSecondaryOam.ui8Bytes[m_ui8Oam2ClearIdx++] = m_ui8OamLatch;"
						"m_ui8Oam2ClearIdx %= sizeof( m_soSecondaryOam.ui8Bytes );\r\n";
					}
				}
				// Sprite evaluation.
				if ( (_uX >= LSN_SPR_EVAL_START && _uX < LSN_RIGHT) ) {
					sRet += "\r\n"
					"Pixel_Evaluation_Sprite<";
					sRet += _uX == LSN_SPR_EVAL_START ? "true" : "false";
					sRet += ", ";
					sRet += (_uX & 1) ? "true" : "false";
					sRet += ">();\r\n";
				}
				// Sprite fetches.
				if ( (_uX >= LSN_RIGHT && _uX < LSN_NEXT_TWO) ) {
					sRet += "\r\n"
					"Pixel_Fetch_Sprite<" + std::to_string( (_uX - LSN_RIGHT) / 8 ) + ", " + std::to_string( (_uX - LSN_RIGHT) % 8 ) + ">();	// Sprite fetches (257-320).\r\n";
				}

				if ( (_uX >= (LSN_LEFT + 1) && _uX < LSN_RIGHT) ) {
					sRet += "\r\n"
					"if ( Rendering() ) {\r\n"
					"	for ( uint8_t I = m_ui8ThisLineSpriteCount; I--; ) {\r\n"
					"		if ( m_asActiveSprites.ui8X[I] ) {\r\n"
					"			--m_asActiveSprites.ui8X[I];\r\n"
					"		}\r\n"
					"		else {\r\n"
					"			m_asActiveSprites.ui8ShiftLo[I] <<= 1;\r\n"
					"			m_asActiveSprites.ui8ShiftHi[I] <<= 1;\r\n"
					"		}\r\n"
					"	}\r\n"
					"}\r\n";
				}
			}
			if ( _uY == ui61RenderHeight && _uX == 0 ) {
				sRet += "\r\n"
				"m_ui8ThisLineSpriteCount = 0;\r\n";
			}


			// Shifting and transferring to shifters.
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				((_uX >= LSN_LEFT && _uX < LSN_RIGHT) || (_uX >= LSN_NEXT_TWO && _uX < LSN_DUMMY_BEGIN)) ) {
				sRet += "\r\n"
				"if ( Rendering() ) {\r\n"
				"	m_ui16ShiftPatternLo <<= 1;\r\n"
				"	m_ui16ShiftPatternHi <<= 1;\r\n"
				"	m_ui16ShiftAttribLo <<= 1;\r\n"
				"	m_ui16ShiftAttribHi <<= 1;\r\n";
				if ( (_uX - LSN_LEFT) % 8 == 0 ) {
					sRet += "\r\n"
				"	m_ui16ShiftPatternLo = (m_ui16ShiftPatternLo & 0xFF00) | m_ui8NextTileLsb;\r\n"
				"	m_ui16ShiftPatternHi = (m_ui16ShiftPatternHi & 0xFF00) | m_ui8NextTileMsb;\r\n"
				"\r\n"
				"	m_ui16ShiftAttribLo  = (m_ui16ShiftAttribLo & 0xFF00) | ((m_ui8NextTileAttribute & 0b01) ? 0xFF : 0x00);\r\n"
				"	m_ui16ShiftAttribHi  = (m_ui16ShiftAttribHi & 0xFF00) | ((m_ui8NextTileAttribute & 0b10) ? 0xFF : 0x00);\r\n";
				}
				sRet +=	
				"}\r\n";
			}


			// Render.
			{
				uint16_t ui16X, ui16Y;
				if ( CycleToRenderTarget( _uX, _uY, ui16X, ui16Y ) ) {
					sRet += "\r\n"
					"RenderPixel();\r\n";
				}
			}

			// Background processing (1-256, 321-336).
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				((_uX >= LSN_LEFT && _uX < LSN_RIGHT) || (_uX >= LSN_NEXT_TWO && _uX < LSN_DUMMY_BEGIN)) ) {
				// Load background data.
				{
					if ( (_uX - LSN_LEFT) % 8 == 0 ) {
						sRet += "\r\n"
						"// LSN_PPU_NAMETABLES = 0x2000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 1 ) {
						sRet += "\r\n"
						"m_ui8NextTileId = m_ui8NtAtBuffer;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 2 ) {
						sRet += "\r\n"
						"// LSN_PPU_NAMETABLES = 0x2000.\r\n"
						"// LSN_PPU_ATTRIBUTE_TABLE_OFFSET = 0x03C0.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( (LSN_PPU_NAMETABLES + LSN_PPU_ATTRIBUTE_TABLE_OFFSET) | (m_paPpuAddrV.s.ui16NametableY << 11) |\r\n"
						"	(m_paPpuAddrV.s.ui16NametableX << 10) |\r\n"
						"	((m_paPpuAddrV.s.ui16CourseY >> 2) << 3) |\r\n"
						"	(m_paPpuAddrV.s.ui16CourseX >> 2) );\r\n"
						"if ( m_paPpuAddrV.s.ui16CourseY & 0x2 ) { m_ui8NtAtBuffer >>= 4; }\r\n"
						"if ( m_paPpuAddrV.s.ui16CourseX & 0x2 ) { m_ui8NtAtBuffer >>= 2; }\r\n"
						"m_ui8NtAtBuffer &= 0x3;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 3 ) {
						sRet += "\r\n"
						"m_ui8NextTileAttribute = m_ui8NtAtBuffer;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 4 ) {
						sRet += "\r\n"
						"// LSN_PPU_PATTERN_TABLES = 0x0000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +\r\n"
						"	(static_cast<uint16_t>(m_ui8NextTileId) << 4) +\r\n"
						"	(m_paPpuAddrV.s.ui16FineY) +\r\n"
						"	0) );\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 5 ) {
						sRet += "\r\n"
						"m_ui8NextTileLsb = m_ui8NtAtBuffer;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 6 ) {
						sRet += "\r\n"
						"// LSN_PPU_PATTERN_TABLES = 0x0000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_PATTERN_TABLES | ((m_pcPpuCtrl.s.ui8BackgroundTileSelect << 12) +\r\n"
						"	(static_cast<uint16_t>(m_ui8NextTileId) << 4) +\r\n"
						"	(m_paPpuAddrV.s.ui16FineY) +\r\n"
						"	8) );\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 7 ) {
						sRet += "\r\n"
						"m_ui8NextTileMsb = m_ui8NtAtBuffer;\r\n";
					}
				}
			}

			// Garbage fetches (257-320).
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				(_uX >= LSN_RIGHT && _uX < LSN_NEXT_TWO) ) {
				{
					// This has 2 pttern fetches instead of a pattern fetch and then an attribute fetch.
					// It also sets OAMADDR to 0.
					if ( (_uX - LSN_LEFT) % 8 == 0 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n"
						"// LSN_PPU_NAMETABLES = 0x2000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );	// Garbage fetches (257-320).\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 1 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n"
						"/*m_ui8NextTileId = */m_ui8NtAtBuffer;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 2 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n"
						"// LSN_PPU_NAMETABLES = 0x2000.\r\n"
						"m_ui8NtAtBuffer = m_bBus.Read( LSN_PPU_NAMETABLES | (m_paPpuAddrV.ui16Addr & 0x0FFF) );	// Garbage fetches (257-320).\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 3 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n"
						"/*m_ui8NextTileId = */m_ui8NtAtBuffer;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 4 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 5 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 6 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n";
					}
					if ( (_uX - LSN_LEFT) % 8 == 7 ) {
						sRet += "\r\n"
						"m_ui8OamAddr = 0;\r\n";
					}
				}
			}

			
			// Increase H and V.
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				((_uX >= LSN_LEFT && _uX < LSN_RIGHT) || (_uX >= LSN_NEXT_TWO && _uX < LSN_DUMMY_BEGIN)) ) {
				if ( (_uX - LSN_LEFT) % 8 == 7 ) {
					sRet += "\r\n"
					"// Increase v.H.\r\n"
					"if ( Rendering() ) {\r\n"
					"	// m_paPpuAddrV.s.ui16CourseX is only 5 bits so wrapping is automatic with the increment.  Check if the nametable needs to be swapped before the increment.\r\n"
					"	if ( m_paPpuAddrV.s.ui16CourseX == 31 ) {\r\n"
					"		// m_paPpuAddrV.s.ui16NametableX is 1 bit, so just toggle.\r\n"
					"		m_paPpuAddrV.s.ui16NametableX = !m_paPpuAddrV.s.ui16NametableX;\r\n"
					"	}\r\n"
					"	++m_paPpuAddrV.s.ui16CourseX;\r\n"
					"}\r\n";
				}
				if ( _uX == LSN_RIGHT - 1 ) {
					sRet += "\r\n"
					"// Increase v.V.\r\n"
					"if ( Rendering() ) {\r\n"
					"	if ( m_paPpuAddrV.s.ui16FineY < 7 ) {\r\n"
					"		++m_paPpuAddrV.s.ui16FineY;\r\n"
					"	}\r\n"
					"	else {\r\n"
					"		m_paPpuAddrV.s.ui16FineY = 0;\r\n"
					"		// Wrap and increment the course.\r\n"
					"		// Do we need to swap vertical nametable targets?\r\n"
					"		switch ( m_paPpuAddrV.s.ui16CourseY ) {\r\n"
					"			case 29 : {\r\n"
					"				// Wrap the course offset and flip the nametable bit.\r\n"
					"				m_paPpuAddrV.s.ui16CourseY = 0;\r\n"
					"				m_paPpuAddrV.s.ui16NametableY = ~m_paPpuAddrV.s.ui16NametableY;\r\n"
					"				break;\r\n"
					"			}\r\n"
					"			case 31 : {\r\n"
					"				// We are in attribute memory.  Reset but without flipping the nametable.\r\n"
					"				m_paPpuAddrV.s.ui16CourseY = 0;\r\n"
					"				break;\r\n"
					"			}\r\n"
					"			default : {\r\n"
					"				// Somewhere between.  Just increment.\r\n"
					"				++m_paPpuAddrV.s.ui16CourseY;\r\n"
					"			}\r\n"
					"		}\r\n"
					"	}\r\n"
					"}\r\n";
				}
			}

			// Transfer horizontal T -> V.
			if ( ((_uY >= 0 && _uY < ui61RenderHeight) || (_uY == _tDotHeight - 1)) &&
				(_uX == LSN_RIGHT) ) {
				sRet += "\r\n"
				"if ( Rendering() ) {\r\n"
				"	m_paPpuAddrV.s.ui16NametableX = m_paPpuAddrT.s.ui16NametableX;\r\n"
				"	m_paPpuAddrV.s.ui16CourseX = m_paPpuAddrT.s.ui16CourseX;\r\n"
				"}\r\n";
			}

			// Copy vertical T into V.
			if ( (_uY == _tDotHeight - 1) && (_uX >= 280 && _uX < 305) ) {
				sRet += "\r\n"
				"if ( Rendering() ) {\r\n"
				"	m_paPpuAddrV.s.ui16FineY = m_paPpuAddrT.s.ui16FineY;\r\n"
				"	m_paPpuAddrV.s.ui16NametableY = m_paPpuAddrT.s.ui16NametableY;\r\n"
				"	m_paPpuAddrV.s.ui16CourseY = m_paPpuAddrT.s.ui16CourseY;\r\n"
				"}\r\n";
			}

			// Swap render targets.
			if ( _uY == ui61RenderHeight && _uX == (1 + _tRenderW) ) {
				sRet += "\r\n"
				"if ( m_pdhHost ) {\r\n"
				"	if ( m_pui8RenderTarget ) {\r\n"
				"		if ( DebugSideDisplay() ) {\r\n"
				"			for ( uint16_t I = 0; I < 2; ++I ) {\r\n"
				"				for ( uint16_t ui16TileY = 0; ui16TileY < 16; ++ui16TileY ) {\r\n"
				"					for ( uint16_t ui16TileX = 0; ui16TileX < 16; ++ui16TileX ) {\r\n"
				"						uint16_t ui16Offset = ui16TileY * 256 + ui16TileX * 16;\r\n"
				"						for ( uint16_t ui16Row = 0; ui16Row < 8; ++ui16Row ) {\r\n"
				"							uint8_t ui8TileLsb = m_bBus.Read( 0x1000 * I + ui16Offset + ui16Row + 0 );\r\n"
				"							uint8_t ui8TileMsb = m_bBus.Read( 0x1000 * I + ui16Offset + ui16Row + 8 );\r\n"
				"							for ( uint16_t ui16Col = 0; ui16Col < 8; ++ui16Col ) {\r\n"
				"								uint8_t ui8Pixel = (ui8TileLsb & 0x01) + (ui8TileMsb & 0x01);\r\n"
				"								ui8TileLsb >>= 1;\r\n"
				"								ui8TileMsb >>= 1;\r\n"
				"								uint16_t ui16X = ui16TileX * 8 + (7 - ui16Col);\r\n"
				"								ui16X += _tRenderW;\r\n"
				"\r\n"
				"								uint16_t ui16Y = I * (16 * 8) + ui16TileY * 8 + ui16Row;\r\n"
				"								ui16Y = (_tRender - 1) - ui16Y;\r\n"
				"								if ( ui16Y < _tRender ) {\r\n"
				"\r\n"
				"									uint8_t * pui8This = &m_pui8RenderTarget[ui16Y*m_stRenderTargetStride+ui16X*3];\r\n"
				"									uint8_t ui8Val = ui8Pixel * (255 / 4);\r\n"
				"									pui8This[0] = pui8This[1] = pui8This[2] = ui8Val;\r\n"
				"								}\r\n"
				"							}\r\n"
				"						}\r\n"
				"					}\r\n"
				"				}\r\n"
				"			}\r\n"
				"		}\r\n"
				"	}\r\n"
				"	m_pdhHost->Swap();\r\n"
				"}\r\n";
				//"Pixel_Swap_Control();\r\n";
			}

			// Set V-blank and friends.
			if ( _uY == (_tPreRender + _tRender + _tPostRender) && _uX == 1 ) {
				sRet += "\r\n"
				"// [1, 241] on NTSC.\r\n"
				"// [1, 241] on PAL.\r\n"
				"// [1, 241] on Dendy.\r\n"
				"m_psPpuStatus.s.ui8VBlank = 1;\r\n"
				"if ( m_pcPpuCtrl.s.ui8Nmi ) {\r\n"
				"	m_pnNmiTarget->Nmi();\r\n"
				"}\r\n";
			}

			// Clear V-blank and friends.
			if ( _uY == _tDotHeight - 1 && _uX == 1 ) {
				sRet += "\r\n"
				"m_psPpuStatus.s.ui8VBlank = 0;\r\n"
				"m_psPpuStatus.s.ui8SpriteOverflow = 0;\r\n"
				"m_psPpuStatus.s.ui8Sprite0Hit = 0;\r\n";
			}


			// End cycle.
			sRet += "\r\n"
			"++m_stCurCycle;\r\n";

			// End frame.
			{
				if ( _uY == _tDotHeight - 1 && _uX == _tDotWidth - 1 ) {
					sRet += "\r\n"
					"if constexpr ( _bOddFrameShenanigans ) {\r\n"
					"	if ( m_ui64Frame & 0x1 ) {\r\n"
					"		m_stCurCycle = 1;\r\n"
					"	}\r\n"
					"	else {\r\n"
					"		m_stCurCycle = 0;\r\n"
					"	}\r\n"
					"}\r\n"
					"else {\r\n"
					"	m_stCurCycle = 0;\r\n"
					"}\r\n"
					"++m_ui64Frame;\r\n";
				}
			}

			return sRet;
#undef LSN_DUMMY_BEGIN
#undef LSN_NEXT_TWO
#undef LSN_SPR_EVAL_START
#undef LSN_RIGHT
#undef LSN_LEFT
		}

		/**
		 * Generates the PPU functions and the code to create the table.
		 */
		void											GenerateCycleFuncs() {
			struct LSN_TABLE_INDICES {
				uint16_t								ui16X;
				uint16_t								ui16Y;

				/*bool									operator < ( const LSN_TABLE_INDICES &_tiOther ) const {
				}*/
			};
			typedef std::vector<LSN_TABLE_INDICES>		CStringList;
			std::map<std::string, CStringList> mMap;

			LSN_TABLE_INDICES tiI = { 0, 0 };
			while ( tiI.ui16Y < _tDotHeight ) {
				while ( tiI.ui16X < _tDotWidth ) {
					std::string sThis = Cycle_Generate( tiI.ui16X, tiI.ui16Y );
					
					auto aInMap = mMap.find( sThis );
					if ( aInMap == mMap.end() ) {
						CStringList slList;
						slList.push_back( tiI );
						mMap.insert( std::make_pair( sThis, slList ) );
					}
					else {
						aInMap->second.push_back( tiI );
					}
					++tiI.ui16X;
				}
				++tiI.ui16Y;
				tiI.ui16X = 0;
			}

			auto FuncName = []( const CStringList &_slDots ) {
				std::string sName = "Cycle_" + std::to_string( _tRegCode ) + "_";
				size_t I = 0;
				for ( ; I < _slDots.size() && I < 10; ++I ) {
					sName += "_" + std::to_string( _slDots[I].ui16X ) + "x" + std::to_string( _slDots[I].ui16Y );
				}
				if ( I < _slDots.size() ) {
					sName += "_X";
				}
				return sName;
			};

			
			::OutputDebugStringA( ("if constexpr ( _tRegCode == " + std::to_string( _tRegCode ) + " ) {\r\n").c_str() );

			for ( auto I = mMap.begin(); I != mMap.end(); ++I ) {
				std::string sTmp;
				sTmp = "	{\r\n";
				sTmp += "		PfCycles pfTmp = &CPpu2C0X::" + FuncName( (*I).second ) + ";\r\n";
				::OutputDebugStringA( sTmp.c_str() );
				sTmp = "";
				for ( size_t J = 0; J < (*I).second.size(); ++J ) {
					sTmp += "		m_cCycle[" + std::to_string( (*I).second[J].ui16Y * _tDotWidth + (*I).second[J].ui16X ) +
						"] = pfTmp;\r\n";
					::OutputDebugStringA( sTmp.c_str() );
					sTmp = "";
				}
				//sTmp += "&CPpu2C0X::" + FuncName( (*I).second ) + ";\r\n";
				sTmp += "	}\r\n";
				::OutputDebugStringA( sTmp.c_str() );
			}
			::OutputDebugStringA( "}\r\n" );

			::OutputDebugStringA( "\r\n\r\n\r\n" );

			for ( auto I = mMap.begin(); I != mMap.end(); ++I ) {
				std::string sTmp = "void LSN_FASTCALL								" + FuncName( (*I).second ) + "() {\r\n" +
					(*I).first +
					"}\r\n\r\n\r\n";
				::OutputDebugStringA( sTmp.c_str() );
			}
		}
#endif	// #ifdef LSN_GEN_PPU

#include "LSNGenFuncsNtsc.inl"
#include "LSNGenFuncsPal.inl"
#include "LSNGenFuncsDendy.inl"
#endif	// #ifdef LSN_TEMPLATE_PPU
	};
	


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	// DEFINITIONS
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	// == Types.
#define LSN_PPU_TYPE( REGION )							LSN_PM_ ## REGION,																									\
														LSN_PM_ ## REGION ## _DOTS_X, LSN_PM_ ## REGION ## _SCANLINES,														\
														LSN_PM_ ## REGION ## _PRERENDER, LSN_PM_ ## REGION ## _RENDER_LINES, LSN_PM_ ## REGION ## _POSTRENDER_LINES,		\
														LSN_PM_ ## REGION ## _RENDER_WIDTH, LSN_PM_ ## REGION ## _H_BORDER
	/**
	 * An NTSC PPU.
	 */
	typedef CPpu2C0X<LSN_PPU_TYPE( NTSC ), true, 1.143>													CNtscPpu;

	/**
	 * A PAL PPU.
	 */
	typedef CPpu2C0X<LSN_PPU_TYPE( PAL ), false, 1.386>													CPalPpu;

	/**
	 * A Dendy PPU.
	 */
	typedef CPpu2C0X<LSN_PPU_TYPE( DENDY ), false, 1.143>												CDendyPpu;

#undef LSN_PPU_TYPE


}	// namespace lsn

#undef LSN_CTRL_NAMETABLE_X
#undef LSN_CTRL_NAMETABLE_Y
