/**
 * Copyright L. Spiro 2023
 *
 * Written by: Shawn (L. Spiro) Wilcoxen
 *
 * Description: Blargg�s NTSC filter.
 */

#include "LSNNtscBlarggFilter.h"
#include <Helpers/LSWHelpers.h>


namespace lsn {

	CNtscBlarggFilter::CNtscBlarggFilter() :
		m_ui32FinalStride( 0 ) {
		nes_ntsc_setup_t nsTmp = nes_ntsc_composite;
		nsTmp.artifacts = 0.5;
		nsTmp.bleed = 0.8;
		nsTmp.fringing = 0.75;
		nsTmp.sharpness = -0.15;
		nsTmp.merge_fields = 1;
		
		::nes_ntsc_init( &m_nnBlarggNtsc, &nsTmp );
	}
	CNtscBlarggFilter::~CNtscBlarggFilter() {
	}


	// == Functions.
	/**
	 * Sets the basic parameters for the filter.
	 *
	 * \param _stBuffers The number of render targets to create.
	 * \param _ui16Width The console screen width.  Typically 256.
	 * \param _ui16Height The console screen height.  Typically 240.
	 * \return Returns the input format requested of the PPU.
	 */
	CDisplayClient::LSN_PPU_OUT_FORMAT CNtscBlarggFilter::Init( size_t _stBuffers, uint16_t _ui16Width, uint16_t _ui16Height ) {
		m_vBasicRenderTarget.resize( _stBuffers );

		m_ui32OutputWidth = _ui16Width;
		m_ui32OutputHeight = _ui16Height;
		m_stStride = size_t( _ui16Width * sizeof( uint16_t ) );

		for ( auto I = m_vBasicRenderTarget.size(); I--; ) {
			m_vBasicRenderTarget[I].resize( m_stStride * _ui16Height );
			for ( auto J = m_vBasicRenderTarget[I].size() / sizeof( uint16_t ); J--; ) {
				(*reinterpret_cast<uint16_t *>(&m_vBasicRenderTarget[I][J*sizeof( uint16_t )])) = 0x0F;
			}
		}

		m_ui32FinalStride = RowStride( NES_NTSC_OUT_WIDTH( _ui16Width ), OutputBits() );
		m_vFilteredOutput.resize( m_ui32FinalStride * _ui16Height );

		return InputFormat();
	}

	/**
	 * Tells the filter that rendering to the source buffer has completed and that it should filter the results.  The final buffer, along with
	 *	its width, height, bit-depth, and stride, are returned.
	 *
	 * \param _pui8Input The buffer to be filtered, which will be a pointer to one of the buffers returned by OutputBuffer() previously.  Its format will be that returned in InputFormat().
	 * \param _ui32Width On input, this is the width of the buffer in pixels.  On return, it is filled with the final width, in pixels, of the result.
	 * \param _ui32Height On input, this is the height of the buffer in pixels.  On return, it is filled with the final height, in pixels, of the result.
	 * \param _ui16BitDepth On input, this is the bit depth of the buffer.  On return, it is filled with the final bit depth of the result.
	 * \param _ui32Stride On input, this is the stride of the buffer.  On return, it is filled with the final stride, in bytes, of the result.
	 * \param _ui64PpuFrame The PPU frame associated with the input data.
	 * \return Returns a pointer to the filtered output buffer.
	 */
	uint8_t * CNtscBlarggFilter::ApplyFilter( uint8_t * _pui8Input, uint32_t &_ui32Width, uint32_t &_ui32Height, uint16_t &/*_ui16BitDepth*/, uint32_t &/*_ui32Stride*/, uint64_t _ui64PpuFrame ) {
		/*uint16_t * pui16In = reinterpret_cast<uint16_t *>(_pui8Input);
		bool bPrint = false;
		if ( bPrint ) {
			::OutputDebugStringA( "uint16_t ui16Palette[256*240] = {\r\n" );
			for ( int Y = 0; Y < 240; ++Y ) {
				std::string sLine;
				for ( int X = 0; X < 256; ++X ) {
					char szBuffer[32];
					std::sprintf( szBuffer, "0x%.4X, ", pui16In[Y*256+X] );
					sLine += szBuffer;
				}
				::OutputDebugStringA( sLine.c_str() );
				::OutputDebugStringA( "\r\n" );
				sLine.clear();
			}
			

			::OutputDebugStringA( "};\r\n" );
		}*/
		::nes_ntsc_blit( &m_nnBlarggNtsc,
			reinterpret_cast<NES_NTSC_IN_T *>(_pui8Input), _ui32Width, (_ui64PpuFrame + 0) & 0b11, 3,
			_ui32Width, _ui32Height,
			m_vFilteredOutput.data(), m_ui32FinalStride );
		_ui32Width = NES_NTSC_OUT_WIDTH( _ui32Width );
		return m_vFilteredOutput.data();
	}

}	// namespace lsn