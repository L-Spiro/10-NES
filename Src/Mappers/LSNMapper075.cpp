/**
 * Copyright L. Spiro 2022
 *
 * Written by: Shawn (L. Spiro) Wilcoxen
 *
 * Description: Mapper 075 implementation.
 */

#include "LSNMapper075.h"

#define LSN_MASK_75				0xFF


namespace lsn {

	CMapper075::CMapper075() {
	}
	CMapper075::~CMapper075() {
	}

	// == Functions.
	/**
	 * Initializes the mapper with the ROM data.  This is usually to allow the mapper to extract information such as the number of banks it has, as well as make copies of any data it needs to run.
	 *
	 * \param _rRom The ROM data.
	 */
	void CMapper075::InitWithRom( LSN_ROM &_rRom ) {
		CMapperBase::InitWithRom( _rRom );
		m_ui8PgmBank = 0;
		m_ui8PgmBank1 = 1;
		m_ui8PgmBank2 = 2;
		m_ui8ChrBank = 0;
		m_ui8ChrBank1 = 1;

	}

	/**
	 * Applies mapping to the CPU and PPU busses.
	 *
	 * \param _pbCpuBus A pointer to the CPU bus.
	 * \param _pbPpuBus A pointer to the PPU bus.
	 */
	void CMapper075::ApplyMap( CCpuBus * _pbCpuBus, CPpuBus * _pbPpuBus ) {
		// PGM bank 0.
		for ( uint32_t I = 0x8000; I < 0xA000; ++I ) {
			_pbCpuBus->SetReadFunc( uint16_t( I ), &CMapperBase::PgmBankRead_2000, this, uint16_t( I - 0x8000 ) );
		}
		// PGM bank 1.
		for ( uint32_t I = 0xA000; I < 0xC000; ++I ) {
			_pbCpuBus->SetReadFunc( uint16_t( I ), &CMapper075::PgmBank1Read_2000, this, uint16_t( I - 0xA000 ) );
		}
		// PGM bank 2.
		for ( uint32_t I = 0xC000; I < 0xE000; ++I ) {
			_pbCpuBus->SetReadFunc( uint16_t( I ), &CMapper075::PgmBank1Read_2000, this, uint16_t( I - 0xC000 ) );
		}

		// Set the reads of the fixed bank at the end.		
		m_stFixedOffset = std::max<size_t>( m_prRom->vPrgRom.size(), 0x2000 ) - 0x2000;
		for ( uint32_t I = 0xE000; I < 0x10000; ++I ) {
			_pbCpuBus->SetReadFunc( uint16_t( I ), &CMapperBase::PgmBankRead_Fixed, this, uint16_t( I - 0xE000 ) );
		}

		// CHR bank 0.
		for ( uint32_t I = 0x0000; I < 0x1000; ++I ) {
			_pbPpuBus->SetReadFunc( uint16_t( I ), &CMapperBase::ChrBankRead_1000, this, uint16_t( I - 0x0000 ) );
		}
		// CHR bank 1.
		for ( uint32_t I = 0x1000; I < 0x2000; ++I ) {
			_pbPpuBus->SetReadFunc( uint16_t( I ), &CMapper075::ChrBank1Read_1000, this, uint16_t( I - 0x1000 ) );
		}

		// Select banks.
		for ( uint32_t I = 0x8000; I < 0x9000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBank8000_8FFF, this, uint16_t( I ) );	// Treated as ROM.
		}
		for ( uint32_t I = 0x9000; I < 0xA000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBank9000_9FFF, this, uint16_t( I ) );	// Treated as ROM.
		}
		for ( uint32_t I = 0xA000; I < 0xB000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBankA000_AFFF, this, uint16_t( I ) );	// Treated as ROM.
		}
		for ( uint32_t I = 0xC000; I < 0xD000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBankC000_CFFF, this, uint16_t( I ) );	// Treated as ROM.
		}
		for ( uint32_t I = 0xE000; I < 0xF000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBankE000_EFFF, this, uint16_t( I ) );	// Treated as ROM.
		}
		for ( uint32_t I = 0xF000; I < 0x10000; ++I ) {
			_pbCpuBus->SetWriteFunc( uint16_t( I ), &CMapper075::SelectBankF000_FFFF, this, uint16_t( I ) );	// Treated as ROM.
		}


		ApplyControllableMirrorMap( _pbPpuBus );
	}

	/**
	 * Reads from PGM ROM using m_ui8PgmBank1 to select a bank.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
	 * \param _pui8Data The buffer from which to read.
	 * \param _ui8Ret The read value.
	 */
	void LSN_FASTCALL CMapper075::PgmBank1Read_2000( void * _pvParm0, uint16_t _ui16Parm1, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		_ui8Ret = pmThis->m_prRom->vPrgRom[_ui16Parm1+(pmThis->m_ui8PgmBank1*0x2000)];
	}

	/**
	 * Reads from PGM ROM using m_ui8PgmBank2 to select a bank.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
	 * \param _pui8Data The buffer from which to read.
	 * \param _ui8Ret The read value.
	 */
	void LSN_FASTCALL CMapper075::PgmBank2Read_2000( void * _pvParm0, uint16_t _ui16Parm1, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		_ui8Ret = pmThis->m_prRom->vPrgRom[_ui16Parm1+(pmThis->m_ui8PgmBank2*0x2000)];
	}

	/**
	 * Reading from the PPU range 0x0000-0x2000 returns a value read from the current 4-kilobyte bank.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to read from _pui8Data.  It is not constant because sometimes reads do modify status registers etc.
	 * \param _pui8Data The buffer from which to read.
	 * \param _ui8Ret The read value.
	 */
	void LSN_FASTCALL CMapper075::ChrBank1Read_1000( void * _pvParm0, uint16_t _ui16Parm1, uint8_t * /*_pui8Data*/, uint8_t &_ui8Ret ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		_ui8Ret = pmThis->m_prRom->vChrRom[pmThis->m_ui8ChrBank1*0x1000+_ui16Parm1];
	}

	/**
	 * Select 8 KB PRG ROM at $8000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBank8000_8FFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		pmThis->m_ui8PgmBank = (_ui8Val & LSN_MASK_75) % (pmThis->m_prRom->vPrgRom.size() / 0x2000);
	}

	/**
	 * Select 8 KB PRG ROM at $A000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBankA000_AFFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		pmThis->m_ui8PgmBank1 = (_ui8Val & LSN_MASK_75) % (pmThis->m_prRom->vPrgRom.size() / 0x2000);
	}

	/**
	 * Select 8 KB PRG ROM at $C000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBankC000_CFFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		pmThis->m_ui8PgmBank2 = (_ui8Val & LSN_MASK_75) % (pmThis->m_prRom->vPrgRom.size() / 0x2000);
	}

	/**
	 * Mirroring  (0: Vertical; 1: Horizontal), High Bit of 4 KB CHR bank at PPU $0000, High Bit of 4 KB CHR bank at PPU $1000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBank9000_9FFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		/*
		 *	7  bit  0
		 *	---------
		 *	.... .BAM
		 *		  |||
		 *		  ||+- Mirroring  (0: Vertical; 1: Horizontal)
		 *		  |+-- High Bit of 4 KB CHR bank at PPU $0000
		 *		  +--- High Bit of 4 KB CHR bank at PPU $1000
		 */
		switch ( _ui8Val & 0b001 ) {
			case 0 : {
				pmThis->m_mmMirror = LSN_MM_VERTICAL;
				break;
			}
			case 1 : {
				pmThis->m_mmMirror = LSN_MM_HORIZONTAL;
				break;
			}
		}
		pmThis->m_ui8ChrBank = (pmThis->m_ui8ChrBank & 0b01111) | ((_ui8Val & 0b00010) << 3);
		pmThis->m_ui8ChrBank1 = (pmThis->m_ui8ChrBank1 & 0b01111) | ((_ui8Val & 0b00100) << 2);

		pmThis->m_ui8ChrBank = (pmThis->m_ui8ChrBank) % (pmThis->m_prRom->vChrRom.size() / 0x1000);
		pmThis->m_ui8ChrBank1 = (pmThis->m_ui8ChrBank1) % (pmThis->m_prRom->vChrRom.size() / 0x1000);
	}

	/**
	 * Low 4 bits of 4 KB CHR bank at PPU $0000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBankE000_EFFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		pmThis->m_ui8ChrBank = (pmThis->m_ui8ChrBank & 0b10000) | (_ui8Val & 0b01111);
		pmThis->m_ui8ChrBank = (pmThis->m_ui8ChrBank) % (pmThis->m_prRom->vChrRom.size() / 0x1000);
	}

	/**
	 * Low 4 bits of 4 KB CHR bank at PPU $1000.
	 *
	 * \param _pvParm0 A data value assigned to this address.
	 * \param _ui16Parm1 A 16-bit parameter assigned to this address.  Typically this will be the address to write to _pui8Data.
	 * \param _pui8Data The buffer to which to write.
	 * \param _ui8Ret The value to write.
	 */
	void LSN_FASTCALL CMapper075::SelectBankF000_FFFF( void * _pvParm0, uint16_t /*_ui16Parm1*/, uint8_t * /*_pui8Data*/, uint8_t _ui8Val ) {
		CMapper075 * pmThis = reinterpret_cast<CMapper075 *>(_pvParm0);
		pmThis->m_ui8ChrBank1 = (pmThis->m_ui8ChrBank1 & 0b10000) | (_ui8Val & 0b01111);
		pmThis->m_ui8ChrBank1 = (pmThis->m_ui8ChrBank1) % (pmThis->m_prRom->vChrRom.size() / 0x1000);
	}

}	// namespace lsn

#undef LSN_MASK_75