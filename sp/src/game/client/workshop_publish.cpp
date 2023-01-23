//=============================================================================//
//
// Purpose: Workshop item manager.
//
//
//=============================================================================//

#include "cbase.h"
#include "filesystem.h"
#include "tier1/fmtstr.h"
#include "tier1/convar.h"
#include <time.h>

#include <vgui/VGUI.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>

#include <ienginevgui.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/FileOpenDialog.h>
#include <vgui_controls/DirectorySelectDialog.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/MessageBox.h>

#include "steam/steam_api.h"
#include "clientsteamcontext.h"

//#include "jpeglib/jpeglib.h"
//#include "libpng/png.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef JPEG_LIB_VERSION
bool JPEGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );
bool JPEGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height );
#endif

#ifdef PNG_LIBPNG_VER
bool PNGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );
bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height );
#endif



static const int g_nTagsGridRowCount = 3;
static const int g_nTagsGridColumnCount = 3;

//
// Workshop tags, displayed on a grid.
// Put NULL to skip grid cells.
//
// Current layout supports 5x3 tags,
// adding more will require changing the layout or implementing a scrollbar on the grid.
//
const char *g_ppWorkshopTags[ g_nTagsGridRowCount ][ g_nTagsGridColumnCount ] =
{
	{ "Custom Map", "Campaign Addon / Mod", "Misc Map" },
	{ "Weapon", "NPC", "Item" },
	{ "Sound", "Script", "Misc Asset" },
};


using namespace vgui;

const char *GetResultDesc( EResult eResult )
{
	switch ( eResult )
	{
	case k_EResultOK:
	{
		return "The operation completed successfully.";
	}
	case k_EResultFail:
	{
		return "Generic failure.";
	}
	case k_EResultNoConnection:
	{
		return "Failed network connection.";
	}
	case k_EResultInsufficientPrivilege:
	{
		return "The user is currently restricted from uploading content due to a hub ban, account lock, or community ban. They would need to contact Steam Support.";
	}
	case k_EResultBanned:
	{
		return "The user doesn't have permission to upload content to this hub because they have an active VAC or Game ban.";
	}
	case k_EResultTimeout:
	{
		return "The operation took longer than expected. Have the user retry the creation process.";
	}
	case k_EResultNotLoggedOn:
	{
		return "The user is not currently logged into Steam.";
	}
	case k_EResultServiceUnavailable:
	{
		return "The workshop server hosting the content is having issues - have the user retry.";
	}
	case k_EResultInvalidParam:
	{
		return "One of the submission fields contains something not being accepted by that field.";
	}
	case k_EResultAccessDenied:
	{
		return "There was a problem trying to save the title and description. Access was denied.";
	}
	case k_EResultLimitExceeded:
	{
		return "The user has exceeded their Steam Cloud quota. Have them remove some items and try again.";
	}
	case k_EResultFileNotFound:
	{
		return "The uploaded file could not be found.";
	}
	case k_EResultDuplicateRequest:
	{
		return "The file was already successfully uploaded. The user just needs to refresh.";
	}
	case k_EResultDuplicateName:
	{
		return "The user already has a Steam Workshop item with that name.";
	}
	case k_EResultServiceReadOnly:
	{
		return "Due to a recent password or email change, the user is not allowed to upload new content. Usually this restriction will expire in 5 days, but can last up to 30 days if the account has been inactive recently.";
	}
	case k_EResultIOFailure:
	{
		return "Generic IO failure.";
	}
	case k_EResultDiskFull:
	{
		return "Operation canceled - not enough disk space.";
	}
	default: return "";
	}
}


enum
{
	k_ERemoteStoragePublishedFileVisibilityUnlisted_149 = 3
};




class CWorkshopPublishDialog;
class CItemPublishDialog;
class CModalMessageBox;
class CUploadProgressBox;
class CPreviewImage;
class CDeleteMessageBox;
class CSimpleGrid;

CWorkshopPublishDialog *g_pWorkshopPublish = NULL;


class CWorkshopPublishDialog : public Frame
{
	typedef Frame BaseClass;

	friend class CItemPublishDialog;

public:
	CWorkshopPublishDialog();

	void ApplySchemeSettings( IScheme *pScheme );
	void PerformLayout();
	void OnCommand( const char *command );
	void OnClose();

public:
	void QueryItem( PublishedFileId_t );
	bool QueryAll();
	void CreateItem();
	void SubmitItemUpdate( PublishedFileId_t );
	void DeleteItem( PublishedFileId_t );

	UGCUpdateHandle_t GetItemUpdate() const { return m_hItemUpdate; }

	void Refresh()
	{
		m_pList->RemoveAll();
		m_nQueryPage = 1;

		if ( QueryAll() )
		{
			m_nQueryTime = system()->GetTimeMillis();
			m_pRefresh->SetEnabled( false );
		}
	}

protected:
	ListPanel *m_pList;
	Button *m_pAdd;
	Button *m_pDelete;
	Button *m_pEdit;
	Button *m_pRefresh;
	CItemPublishDialog *m_pItemPublishDialog;
	UGCUpdateHandle_t m_hItemUpdate;

	DHANDLE< CModalMessageBox > m_pLoadingMessageBox;
	DHANDLE< CDeleteMessageBox > m_pDeleteMessageBox;
	PublishedFileId_t m_nItemToDelete;

	uint32 m_nQueryPage;
	long m_nQueryTime;

private:
	void OnCreateItemResult( CreateItemResult_t *pResult, bool bIOFailure );
	void OnSteamUGCQueryCompleted( SteamUGCQueryCompleted_t *pResult, bool bIOFailure );
	void OnSubmitItemUpdateResult( SubmitItemUpdateResult_t *pResult, bool bIOFailure );
	void OnDeleteItemResult( RemoteStorageDeletePublishedFileResult_t *pResult, bool bIOFailure );
	void OnSteamUGCRequestUGCDetailsResult( SteamUGCRequestUGCDetailsResult_t *pResult, bool bIOFailure );
	void OnRemoteStorageDownloadUGCResult( RemoteStorageDownloadUGCResult_t *pResult, bool bIOFailure );

	CCallResult< CWorkshopPublishDialog, CreateItemResult_t > m_CreateItemResult;
	CCallResult< CWorkshopPublishDialog, SteamUGCQueryCompleted_t > m_SteamUGCQueryCompleted;
	CCallResult< CWorkshopPublishDialog, SubmitItemUpdateResult_t > m_SubmitItemUpdateResult;
	CCallResult< CWorkshopPublishDialog, RemoteStorageDeletePublishedFileResult_t > m_DeleteItemResult;
	CCallResult< CWorkshopPublishDialog, SteamUGCRequestUGCDetailsResult_t > m_SteamUGCRequestUGCDetailsResult;
	CCallResult< CWorkshopPublishDialog, RemoteStorageDownloadUGCResult_t > m_RemoteStorageDownloadUGCResult;
};


class CItemPublishDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( CItemPublishDialog, Frame );

	friend class CWorkshopPublishDialog;

public:
	CItemPublishDialog( Panel* );
	void ApplySchemeSettings( IScheme *pScheme );
	void PerformLayout();
	void OnCommand( const char *command );
	void OnClose();

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	MESSAGE_FUNC_CHARPTR( OnDirectorySelected, "DirectorySelected", dir );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	void SetPreviewImage( const char *filename );
	void SetPreviewImage( CUtlBuffer &file, const char *filename );
	void OnPublish();
	void UpdateFields( KeyValues * );

private:
	PublishedFileId_t m_item;

	Label *m_pTitleLabel;
	Label *m_pDescLabel;
	Label *m_pChangesLabel;
	Label *m_pPreviewLabel;
	Label *m_pContentLabel;
	Label *m_pVisibilityLabel;
	TextEntry *m_pTitleInput;
	TextEntry *m_pDescInput;
	TextEntry *m_pChangesInput;
	TextEntry *m_pPreviewInput;
	TextEntry *m_pContentInput;
	ComboBox *m_pVisibility;
	CPreviewImage *m_pPreview;
	Button *m_pPreviewBrowse;
	Button *m_pContentBrowse;
	Button *m_pClose;
	Button *m_pPublish;
	Label *m_pTagsLabel;
	CSimpleGrid *m_pTagsGrid;

	DHANDLE< CUploadProgressBox > m_pProgressUpload;
};

class CPreviewImage : public Panel
{
public:
	bool m_bLoading;

public:
	CPreviewImage( Panel* pParent, const char *name ) : Panel( pParent, name ),
		m_iTexture(-1),
		m_nDrawWidth(0),
		m_nDrawHeight(0),
		m_s1(1.0f),
		m_t1(1.0f),
		m_bLoading(0)
	{
	}

	~CPreviewImage()
	{
		if ( m_iTexture > 0 )
		{
			surface()->DestroyTextureID( m_iTexture );
		}
	}

	void Paint()
	{
		if ( m_iTexture > 0 )
		{
			surface()->DrawSetColor( 255, 255, 255, 255 );
			surface()->DrawSetTexture( m_iTexture );
			surface()->DrawTexturedSubRect( 0, 0, m_nDrawWidth, m_nDrawHeight, 0.0f, 0.0f, m_s1, m_t1 );
		}
		else
		{
			int wide, tall;
			int textWide, textTall;
			const wchar_t *text;
			HFont font = scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultFixedOutline", false );
			GetSize( wide, tall );

			// Transparent background box
			surface()->DrawSetColor( 0, 0, 0, 127 );
			surface()->DrawFilledRect( 0, 0, wide, tall );

			if ( m_bLoading )
			{
				text = g_pVGuiLocalize->Find( "#GameUI_Loading" );
			}
			else
			{
				text = g_pVGuiLocalize->Find( "#WorkshopMgr_NoImage" );
			}

			surface()->GetTextSize( font, text, textWide, textTall );
			surface()->DrawSetTextColor( 255, 255, 255, 255 );
			surface()->DrawSetTextFont( font );
			surface()->DrawSetTextPos( (wide - textWide) >> 1, (tall - textTall) >> 1 );
			surface()->DrawPrintText( text, wcslen(text) );
		}
	}

	// Open file browser when empty image box is clicked
	void OnMousePressed( MouseCode code )
	{
		if ( code != MOUSE_LEFT )
			return;

		if ( m_iTexture > 0 || m_bLoading )
			return;

		CItemPublishDialog *pItemPublishDialog = (CItemPublishDialog *)GetParent();
		pItemPublishDialog->OnCommand( "PreviewBrowse" );
	}

	void RemoveImage()
	{
		if ( m_iTexture > 0 )
		{
			surface()->DestroyTextureID( m_iTexture );
			m_iTexture = -1;
		}

		m_bLoading = false;
	}

	void SetJPEGImage( const char *filename )
	{
#ifdef JPEG_LIB_VERSION
		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( JPEGtoRGBA( filename, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded jpeg %dx%d '%s'\n", imageWidth, imageHeight, V_GetFileName( filename ) );
		}
		else
		{
			RemoveImage();
		}
#else
		if ( g_pFullFileSystem->FileExists( filename ) )
		{
			m_bLoading = true;
		}
#endif
	}

	void SetJPEGImage( CUtlBuffer &fileBuffer )
	{
#ifdef JPEG_LIB_VERSION
		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( JPEGtoRGBA( fileBuffer, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded jpeg %dx%d\n", imageWidth, imageHeight );
		}
		else
		{
			RemoveImage();
		}
#else
		m_bLoading = true;
#endif
	}

	void SetPNGImage( const char *filename )
	{
#ifdef PNG_LIBPNG_VER
		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( PNGtoRGBA( filename, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded png %dx%d '%s'\n", imageWidth, imageHeight, V_GetFileName( filename ) );
		}
		else
		{
			RemoveImage();
		}
#else
		if ( g_pFullFileSystem->FileExists( filename ) )
		{
			m_bLoading = true;
		}
#endif
	}

	void SetPNGImage( CUtlBuffer &fileBuffer )
	{
#ifdef PNG_LIBPNG_VER
		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( PNGtoRGBA( fileBuffer, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded png %dx%d\n", imageWidth, imageHeight );
		}
		else
		{
			RemoveImage();
		}
#else
		m_bLoading = true;
#endif
	}

private:
	void SetImageRGBA( const unsigned char *image, int imageWidth, int imageHeight )
	{
		// Always reset texture
		if ( m_iTexture > 0 )
			surface()->DestroyTextureID( m_iTexture );

		m_iTexture = surface()->CreateNewTextureID( true );

		// This will pad the image to the nearest power of 2
		surface()->DrawSetTextureRGBA( m_iTexture, image, imageWidth, imageHeight, true, true );

		// Calculate the padding and don't draw it
		int texWidth, texHeight;
		surface()->DrawGetTextureSize( m_iTexture, texWidth, texHeight );

		m_s1 = 1.0f - (float)(texWidth - imageWidth) / (float)texWidth;
		m_t1 = 1.0f - (float)(texHeight - imageHeight) / (float)texHeight;

		// Fit image to panel bounds
		int panelWidth, panelHeight;
		GetSize( panelWidth, panelHeight );

		if ( imageWidth > imageHeight )
		{
			// Fit to width
			m_nDrawWidth = min( panelWidth, imageWidth );
			m_nDrawHeight = imageHeight * m_nDrawWidth / imageWidth;
		}
		else
		{
			// Fit to height
			m_nDrawHeight = min( panelHeight, imageHeight );
			m_nDrawWidth = imageWidth * m_nDrawHeight / imageHeight;
		}
	}

private:
	int m_iTexture;
	int m_nDrawWidth, m_nDrawHeight;
	float m_s1, m_t1;
};


class CModalMessageBox : public Frame
{
	typedef Frame BaseClass;

public:
	CModalMessageBox( Panel *pParent, const char *msg );
	void ApplySchemeSettings( IScheme *pScheme );
	void PerformLayout();

private:
	Label *m_pLabel;
};


class CUploadProgressBox : public Frame
{
	typedef Frame BaseClass;

public:
	CUploadProgressBox( Panel *pParent );
	void ApplySchemeSettings( IScheme *pScheme );
	void PerformLayout();
	void OnTick();

private:
	EItemUpdateStatus m_status;
	Label *m_pLabel;
	ProgressBar *m_pProgressBar;
};


class CDeleteMessageBox : public MessageBox
{
	typedef MessageBox BaseClass;

public:
	CDeleteMessageBox( Panel *pParent, const wchar_t *title, const wchar_t *msg );
	void ApplySchemeSettings( IScheme *pScheme );
	void OnTick();
};


class CSimpleGrid : public Panel
{
	typedef Panel BaseClass;

public:
	CSimpleGrid( Panel *pParent, const char *name );
	void LayoutElements();
	void SetDimensions( int row, int col );
	void SetElementSize( int w, int t );
	void SetCellSize( int w, int t );
	Panel *GetElement( int row, int col );
	void SetElement( int row, int col, Panel *panel );

private:
	CUtlVector< Panel* > m_Elements;
	int m_rows;
	int m_columns;
	int m_cellWide;
	int m_cellTall;
	int m_panelWide;
	int m_panelTall;
};


#ifdef JPEG_LIB_VERSION
struct JpegErrorHandler_t
{
	struct jpeg_error_mgr mgr;
	jmp_buf jmp;
};

void JpegErrorExit( j_common_ptr cinfo )
{
	char msg[ JMSG_LENGTH_MAX ];
	(cinfo->err->format_message)( cinfo, msg );
	Warning( "%s\n", msg );

	longjmp( ((JpegErrorHandler_t*)(cinfo->err))->jmp, 1 );
}

// Read JPEG from CUtlBuffer
struct JpegSourceMgr_t : jpeg_source_mgr
{
	// See libjpeg.txt, jdatasrc.c

	static void _init_source(j_decompress_ptr cinfo) {}
	static void _term_source(j_decompress_ptr cinfo) {}
	static boolean _fill_input_buffer(j_decompress_ptr cinfo) { return 0; }
	static void _skip_input_data( j_decompress_ptr cinfo, long num_bytes )
	{
		JpegSourceMgr_t *src = (JpegSourceMgr_t*)cinfo->src;
		src->next_input_byte += (size_t)num_bytes;
		src->bytes_in_buffer -= (size_t)num_bytes;
	}

	void SetSourceMgr( j_decompress_ptr cinfo, CUtlBuffer *pBuffer )
	{
		cinfo->src = this;
		init_source = _init_source;
		fill_input_buffer = _fill_input_buffer;
		skip_input_data = _skip_input_data;
		resync_to_restart = jpeg_resync_to_restart;
		term_source = _term_source;

		bytes_in_buffer = pBuffer->TellMaxPut();
		next_input_byte = (const JOCTET*)pBuffer->Base();
	}
};

//
// Read a JPEG image from file into buffer as RGBA.
//
bool JPEGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height )
{
	// Read the whole image to memory
	CUtlBuffer fileBuffer;

	if ( !g_pFullFileSystem->ReadFile( filename, NULL, fileBuffer ) )
	{
		Warning( "Failed to read JPEG file (%s)\n", filename );
		return false;
	}

	return JPEGtoRGBA( fileBuffer, buffer, width, height );
}

//
// TODO: Error codes
//
bool JPEGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height )
{
	Assert( sizeof(JSAMPLE) == sizeof(byte) );
	Assert( !buffer.IsReadOnly() );

	struct jpeg_decompress_struct cinfo;
	struct JpegErrorHandler_t jerr;
	JpegSourceMgr_t src_mgr;

	cinfo.err = jpeg_std_error( &jerr.mgr );
	jerr.mgr.error_exit = JpegErrorExit;

	if ( setjmp( jerr.jmp ) )
	{
		jpeg_destroy_decompress( &cinfo );
		return false;
	}

	jpeg_create_decompress( &cinfo );
	src_mgr.SetSourceMgr( &cinfo, &fileBuffer );

	if ( jpeg_read_header( &cinfo, TRUE ) != JPEG_HEADER_OK )
	{
		Warning( "Bad JPEG signature\n" );
		jpeg_destroy_decompress( &cinfo );
		return false;
	}

	if ( !jpeg_start_decompress( &cinfo ) )
	{
		jpeg_destroy_decompress( &cinfo );
		return false;
	}

	int image_width = width = cinfo.output_width;
	int image_height = height = cinfo.output_height;
	int image_components = cinfo.output_components + 1; // RGB + A

	if ( image_components != 4 )
	{
		Warning( "JPEG is not RGB\n" );
		jpeg_destroy_decompress( &cinfo );
		return false;
	}

	buffer.Init( 0, image_components * image_width * image_height );

	while ( cinfo.output_scanline < cinfo.output_height )
	{
		JSAMPROW pRow = buffer.Base() + image_components * image_width * cinfo.output_scanline;
		jpeg_read_scanlines( &cinfo, &pRow, 1 );

		// Expand RGB to RGBA
		for ( int i = image_width; i--; )
		{
			pRow[i*4+0] = pRow[i*3+0];
			pRow[i*4+1] = pRow[i*3+1];
			pRow[i*4+2] = pRow[i*3+2];
			pRow[i*4+3] = 0xFF;
		}
	}

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	Assert( jerr.mgr.num_warnings == 0 );
	return true;
}
#endif

#ifdef PNG_LIBPNG_VER
void ReadPNG_CUtlBuffer( png_structp png_ptr, png_bytep data, size_t length )
{
	if ( !png_ptr )
		return;

	CUtlBuffer *pBuffer = (CUtlBuffer *)png_get_io_ptr( png_ptr );

	if ( (size_t)pBuffer->TellMaxPut() < ( (size_t)pBuffer->TellGet() + length ) ) // CUtlBuffer::CheckGet()
	{
		//png_error( png_ptr, "read error" );
		png_longjmp( png_ptr, 1 );
	}

	pBuffer->Get( data, length );
}

//
// Read a PNG image from file into buffer as RGBA.
//
bool PNGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height )
{
	// Read the whole image to memory
	CUtlBuffer fileBuffer;

	if ( !g_pFullFileSystem->ReadFile( filename, NULL, fileBuffer ) )
	{
		Warning( "Failed to read PNG file (%s)\n", filename );
		return false;
	}

	return PNGtoRGBA( fileBuffer, buffer, width, height );
}

//
// TODO: Error codes
//
bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height )
{
	if ( png_sig_cmp( (png_const_bytep)fileBuffer.Base(), 0, 8 ) )
	{
		Warning( "Bad PNG signature\n" );
		return false;
	}

	png_bytepp row_pointers = NULL;

	png_structp png_ptr = png_create_read_struct( png_get_libpng_ver(NULL), NULL, NULL, NULL );
	png_infop info_ptr = png_create_info_struct( png_ptr );

	if ( !info_ptr || !png_ptr )
	{
		Warning( "Out of memory reading PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		Warning( "Failed to read PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		if ( row_pointers )
			free( row_pointers );
		return false;
	}

	png_set_read_fn( png_ptr, &fileBuffer, ReadPNG_CUtlBuffer );
	png_read_info( png_ptr, info_ptr );

	png_uint_32 image_width, image_height;
	int bit_depth, color_type;

	png_get_IHDR( png_ptr, info_ptr, &image_width, &image_height, &bit_depth, &color_type, NULL, NULL, NULL );

	width = image_width;
	height = image_height;

	// expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
	// transparency chunks to full alpha channel; strip 16-bit-per-sample
	// images to 8 bits per sample; and convert grayscale to RGB[A]

	if ( color_type == PNG_COLOR_TYPE_PALETTE )
		png_set_expand(png_ptr);
	if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
		png_set_expand(png_ptr);
	if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
		png_set_expand(png_ptr);
#ifdef PNG_READ_16_TO_8_SUPPORTED
	if ( bit_depth == 16 )
	#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png_ptr);
	#else
		png_set_strip_16(png_ptr);
	#endif
#endif
	if ( color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb(png_ptr);

	// Expand RGB to RGBA
	if ( color_type == PNG_COLOR_TYPE_RGB )
		png_set_filler( png_ptr, 0xffff, PNG_FILLER_AFTER );

	png_read_update_info( png_ptr, info_ptr );

	png_uint_32 rowbytes = png_get_rowbytes( png_ptr, info_ptr );
	int channels = (int)png_get_channels( png_ptr, info_ptr );

	if ( channels != 4 )
	{
		Warning( "PNG is not RGBA\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( image_height > ((size_t)(-1)) / rowbytes )
	{
		Warning( "PNG data buffer would be too large\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	buffer.Init( 0, rowbytes * image_height );
	row_pointers = (png_bytepp)malloc( image_height * sizeof(png_bytep) );

	Assert( buffer.Base() && row_pointers );

	if ( !row_pointers )
	{
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	for ( png_uint_32 i = 0; i < image_height; ++i )
		row_pointers[i] = buffer.Base() + i*rowbytes;

	png_read_image( png_ptr, row_pointers );
	//png_read_end( png_ptr, NULL );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	free( row_pointers );

	return true;
}
#endif


// Most recent to oldest
int __cdecl SortTimeUpdated( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return item2.kv->GetInt("timeupdated_raw") - item1.kv->GetInt("timeupdated_raw");
}

// Oldest to most recent
int __cdecl SortTimeCreated( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return item1.kv->GetInt("timecreated_raw") - item2.kv->GetInt("timecreated_raw");
}



CWorkshopPublishDialog::CWorkshopPublishDialog() :
	BaseClass( NULL, "WorkshopPublishDialog", false, false ),
	m_pItemPublishDialog( NULL ),
	m_nItemToDelete( 0 ),
	m_hItemUpdate( k_UGCUpdateHandleInvalid )
{
	Assert( !g_pWorkshopPublish );
	g_pWorkshopPublish = this;

	SetParent( enginevgui->GetPanel(PANEL_TOOLS) );
	SetVisible( true );
	SetTitle( "#WorkshopMgr_BrowsePublishedFiles", false );
	SetDeleteSelfOnClose( true );
	SetMoveable( true );
	SetSizeable( false );

	MakePopup();
	RequestFocus();

	m_pList = new ListPanel( this, "PublishedFileList" );
	m_pList->SetMultiselectEnabled( false );
	m_pList->SetEmptyListText( "#WorkshopMgr_Empty" );
	m_pList->AddColumnHeader( 0, "title", "#WorkshopMgr_Title", 30, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 1, "timeupdated", "#WorkshopMgr_LastUpdated", 44, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 2, "timecreated", "#WorkshopMgr_DateCreated", 44, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 3, "visibility", "#WorkshopMgr_Visibility", 20, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 4, "id", "ID", 24, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );

	m_pList->SetSortFunc( 1, &SortTimeUpdated );
	m_pList->SetSortFunc( 2, &SortTimeCreated );
	m_pList->SetSortColumn( 1 );

	m_pAdd = new Button( this, "Add", "#GameUI_Add", this, "Add" );
	m_pDelete = new Button( this, "Delete", "#GameUI_Delete", this, "Delete" );
	m_pEdit = new Button( this, "Edit", "#WorkshopMgr_ButtonEdit", this, "Edit" );
	m_pRefresh = new Button( this, "Refresh", "#GameUI_Refresh", this, "Refresh" );

	m_pAdd->SetContentAlignment( Label::Alignment::a_center );
	m_pDelete->SetContentAlignment( Label::Alignment::a_center );
	m_pEdit->SetContentAlignment( Label::Alignment::a_center );
	m_pRefresh->SetContentAlignment( Label::Alignment::a_center );

	Refresh();
}

void CWorkshopPublishDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder("MenuBorder") );
}

void CWorkshopPublishDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( 780, 580 );
	MoveToCenterOfScreen();

	m_pList->SetPos( 20, 56 );
	m_pList->SetSize( 620, 504 );

	m_pAdd->SetPos( 664, 440 );
	m_pAdd->SetSize( 92, 24 );

	m_pDelete->SetPos( 664, 470 );
	m_pDelete->SetSize( 92, 24 );

	m_pEdit->SetPos( 664, 500 );
	m_pEdit->SetSize( 92, 24 );

	m_pRefresh->SetPos( 664, 530 );
	m_pRefresh->SetSize( 92, 24 );
}

void CWorkshopPublishDialog::OnClose()
{
	g_pWorkshopPublish = NULL;
	BaseClass::OnClose();
}

void CWorkshopPublishDialog::OnCommand( const char *command )
{
	if ( !V_stricmp( "Add", command ) )
	{
		if ( m_pItemPublishDialog )
		{
			m_pItemPublishDialog->MoveToCenterOfScreen();
			m_pItemPublishDialog->MoveToFront();
			m_pItemPublishDialog->RequestFocus();
			return;
		}

		m_pItemPublishDialog = new CItemPublishDialog( this );

		return;
	}

	if ( !V_stricmp( "Edit", command ) )
	{
		if ( m_pItemPublishDialog )
		{
			m_pItemPublishDialog->MoveToCenterOfScreen();
			m_pItemPublishDialog->MoveToFront();
			m_pItemPublishDialog->RequestFocus();
			return;
		}

		int item = m_pList->GetSelectedItem(0);
		if ( item == -1 )
		{
			Warning("no item is selected\n");
			return;
		}

		m_pItemPublishDialog = new CItemPublishDialog( this );
		QueryItem( m_pList->GetItemData(item)->kv->GetUint64("id") );

		return;
	}

	if ( !V_stricmp( "Refresh", command ) )
	{
		Refresh();
		return;
	}

	if ( !V_stricmp( "Delete", command ) )
	{
		int item = m_pList->GetSelectedItem(0);
		if ( item == -1 )
		{
			Warning("no item is selected\n");
			return;
		}

		if ( m_pDeleteMessageBox )
		{
			Assert(0);
			m_pDeleteMessageBox->Close();
			return;
		}

		m_nItemToDelete = m_pList->GetItemData(item)->kv->GetUint64("id");

		wchar_t wtitle[128];
		g_pVGuiLocalize->ConvertANSIToUnicode( m_pList->GetItemData(item)->kv->GetString("title"), wtitle, sizeof(wtitle) );

		wchar_t wszFrameTitle[256];
		V_snwprintf( wszFrameTitle, sizeof(wszFrameTitle), L"%ls (%llu) %ls", g_pVGuiLocalize->Find("#WorkshopMgr_DeleteTitle"), m_nItemToDelete, wtitle );

		m_pDeleteMessageBox = new CDeleteMessageBox( this, wszFrameTitle, g_pVGuiLocalize->Find("#WorkshopMgr_DeleteMsg") );
		m_pDeleteMessageBox->AddActionSignalTarget( this );
		m_pDeleteMessageBox->SetCommand( "DeleteConfirm" );

		return;
	}

	if ( !V_stricmp( "DeleteConfirm", command ) )
	{
		DeleteItem( m_nItemToDelete );
		m_nItemToDelete = 0;
		return;
	}

	BaseClass::OnCommand( command );
}

void CWorkshopPublishDialog::QueryItem( PublishedFileId_t item )
{
	// NOTE: ISteamUGC::RequestUGCDetails was deprecated in favour of ISteamUGC::CreateQueryUGCDetailsRequest
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUGC()->RequestUGCDetails( item, 5 );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::QueryItem() Steam API call failed\n" );
		return;
	}

	m_SteamUGCRequestUGCDetailsResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnSteamUGCRequestUGCDetailsResult );

	m_pLoadingMessageBox = new CModalMessageBox( m_pItemPublishDialog, "Retrieving file information..." );
}

void CWorkshopPublishDialog::OnSteamUGCRequestUGCDetailsResult( SteamUGCRequestUGCDetailsResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	const SteamUGCDetails_t &details = pResult->m_details;
	KeyValues *data = NULL;

	for ( int i = m_pList->FirstItem(); i != m_pList->InvalidItemID(); i = m_pList->NextItem(i) )
	{
		if ( details.m_nPublishedFileId == m_pList->GetItemData(i)->kv->GetUint64("id") )
		{
			data = m_pList->GetItemData(i)->kv;
			break;
		}
	}

	if ( !data )
	{
		// Cannot fail, it was requested via an item in the list
		Assert(0);
		Warning( "Requested item (%llu) was not found in the list\n", details.m_nPublishedFileId );
		m_pItemPublishDialog->Close();
		return;
	}

	data->SetString( "description", details.m_rgchDescription );

	Assert( m_pItemPublishDialog );
	Assert( m_pLoadingMessageBox );

	m_pLoadingMessageBox->Close();
	m_pItemPublishDialog->UpdateFields( data );

	// Don't set downloaded image if preview was already set
	if ( !m_pItemPublishDialog->m_pPreviewInput->GetTextLength() )
	{
		SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->UGCDownload( details.m_hPreviewFile, 1 );
		if ( hSteamAPICall == k_uAPICallInvalid )
		{
			Warning( "CWorkshopPublishDialog::QueryItem() Steam API call failed\n" );
		}
		else
		{
			m_RemoteStorageDownloadUGCResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult );
			m_pItemPublishDialog->m_pPreview->m_bLoading = true;
		}
	}
}

void CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult( RemoteStorageDownloadUGCResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	m_pItemPublishDialog->m_pPreview->m_bLoading = false;

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );
		return;
	}

	DevMsg( "Downloaded %s '%s'\n", V_pretifymem( pResult->m_nSizeInBytes, 2, true ), pResult->m_pchFileName );

	if ( !m_pItemPublishDialog )
		return; // panel closed before download

	Assert( m_pItemPublishDialog->m_item );

	// Don't set downloaded image if preview was already set
	if ( !m_pItemPublishDialog->m_pPreviewInput->GetTextLength() )
	{
		byte *pImage = (byte*)stackalloc( pResult->m_nSizeInBytes * sizeof(byte) );

		steamapicontext->SteamRemoteStorage()->UGCRead( pResult->m_hFile, pImage, pResult->m_nSizeInBytes, 0, k_EUGCRead_ContinueReadingUntilFinished );

		CUtlBuffer buf( pImage, pResult->m_nSizeInBytes, CUtlBuffer::READ_ONLY );
		m_pItemPublishDialog->SetPreviewImage( buf, pResult->m_pchFileName );
	}
}

bool CWorkshopPublishDialog::QueryAll()
{
	UGCQueryHandle_t hUGCQuery = steamapicontext->SteamUGC()->CreateQueryUserUGCRequest( ClientSteamContext().GetLocalPlayerSteamID().GetAccountID(),
		k_EUserUGCList_Published,
		k_EUGCMatchingUGCType_Items,
		k_EUserUGCListSortOrder_LastUpdatedDesc,
		steamapicontext->SteamUtils()->GetAppID(),
		steamapicontext->SteamUtils()->GetAppID(),
		m_nQueryPage );

	if ( hUGCQuery == k_UGCQueryHandleInvalid )
	{
		Warning( "CWorkshopPublishDialog::Refresh() k_UGCQueryHandleInvalid\n" );
		return false;
	}

	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUGC()->SendQueryUGCRequest( hUGCQuery );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::Refresh() Steam API call failed\n" );
		steamapicontext->SteamUGC()->ReleaseQueryUGCRequest( hUGCQuery );
		return false;
	}

	m_SteamUGCQueryCompleted.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnSteamUGCQueryCompleted );
	return true;
}

void CWorkshopPublishDialog::OnSteamUGCQueryCompleted( SteamUGCQueryCompleted_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnSteamUGCQueryCompleted() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );
		steamapicontext->SteamUGC()->ReleaseQueryUGCRequest( pResult->m_handle );

		m_pRefresh->SetEnabled( true );
		return;
	}

	for ( uint32 i = 0; i < pResult->m_unNumResultsReturned; ++i )
	{
		SteamUGCDetails_t details;

		if ( !steamapicontext->SteamUGC()->GetQueryUGCResult( pResult->m_handle, i, &details ) )
		{
			Warning( "GetQueryUGCResult() failed\n" );
			continue;
		}

		if ( details.m_eResult != k_EResultOK )
		{
			Warning( "GetQueryUGCResult() failed [%i]\n", i, details.m_eResult );
			continue;
		}

		CFmtStrN< 21 > strID( "%llu", details.m_nPublishedFileId );

		int idx = m_pList->GetItem( strID );
		if ( idx == -1 )
		{
			KeyValues *data = new KeyValues( strID );
			idx = m_pList->AddItem( data, (unsigned int)data, false, true );
			data->deleteThis();
		}

		KeyValues *data = m_pList->GetItem( idx );
		Assert( data );

		data->SetUint64( "id", details.m_nPublishedFileId );
		data->SetString( "title", details.m_rgchTitle );
		// Long description is set on QueryItem()
		//data->SetString( "description", details.m_rgchDescription );

		// RFC3339 format
		time_t t = details.m_rtimeUpdated;
		tm *date = localtime( &t );
		data->SetString( "timeupdated", CFmtStrN< 32 >("%d-%02d-%02d %02d:%02d:%02d",
			date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec) );

		t = details.m_rtimeCreated;
		date = localtime( &t );
		data->SetString( "timecreated", CFmtStrN< 32 >("%d-%02d-%02d %02d:%02d:%02d",
			date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec) );

		// for sorting
		data->SetInt( "timeupdated_raw", details.m_rtimeUpdated );
		data->SetInt( "timecreated_raw", details.m_rtimeCreated );

		switch ( (int)details.m_eVisibility )
		{
		case k_ERemoteStoragePublishedFileVisibilityPublic:
			data->SetString( "visibility", "#Workshop_Public" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityFriendsOnly:
			data->SetString( "visibility", "#Workshop_FriendsOnly" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityPrivate:
			data->SetString( "visibility", "#Workshop_Private" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityUnlisted_149:
			data->SetString( "visibility", "#Workshop_Unlisted" );
			break;
		}

		data->SetInt( "visibility_raw", details.m_eVisibility );
		data->SetString( "tags", details.m_rgchTags );

		// NOTE: Truncated tags are not retrievable. Use ISteamUGC::GetQueryUGCTag when possible.
		Assert( !details.m_bTagsTruncated );

		m_pList->ApplyItemChanges( idx );
	}

	steamapicontext->SteamUGC()->ReleaseQueryUGCRequest( pResult->m_handle );

	if ( pResult->m_unNumResultsReturned == kNumUGCResultsPerPage )
	{
		m_nQueryPage++;
		if ( QueryAll() )
			return;
	}

	Msg( "Completed query in %f seconds\n", (float)(system()->GetTimeMillis() - m_nQueryTime) / 1e3f );

	m_pRefresh->SetEnabled( true );
}

void CWorkshopPublishDialog::CreateItem()
{
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUGC()->CreateItem( steamapicontext->SteamUtils()->GetAppID(), EWorkshopFileType::k_EWorkshopFileTypeCommunity );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::CreateItem() Steam API call failed\n" );
		return;
	}

	m_CreateItemResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnCreateItemResult );
}

void CWorkshopPublishDialog::OnCreateItemResult( CreateItemResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnCreateItemResult() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );
		return;
	}

	if ( pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement )
	{
		Warning( "User needs to accept workshop legal agreement\n" );
	}

	m_pItemPublishDialog->m_item = pResult->m_nPublishedFileId;

	SubmitItemUpdate( pResult->m_nPublishedFileId );

	// Update labels
	m_pItemPublishDialog->m_pProgressUpload->OnTick();
}

void CWorkshopPublishDialog::SubmitItemUpdate( PublishedFileId_t id )
{
	Assert( m_pItemPublishDialog );

	char pszTitle[ k_cchPublishedDocumentTitleMax - sizeof(wchar_t) ];
	m_pItemPublishDialog->m_pTitleInput->GetText( pszTitle, sizeof(pszTitle) );

	char pszDesc[ k_cchPublishedDocumentDescriptionMax - sizeof(wchar_t) ];
	m_pItemPublishDialog->m_pDescInput->GetText( pszDesc, sizeof(pszDesc) );

	char pszChanges[ k_cchPublishedDocumentChangeDescriptionMax - sizeof(wchar_t) ];
	m_pItemPublishDialog->m_pChangesInput->GetText( pszChanges, sizeof(pszChanges) );

	char pszContent[ MAX_PATH ];
	m_pItemPublishDialog->m_pContentInput->GetText( pszContent, sizeof(pszContent) );

	char pszPreview[ MAX_PATH ];
	m_pItemPublishDialog->m_pPreviewInput->GetText( pszPreview, sizeof(pszPreview) );

	int visibility = m_pItemPublishDialog->m_pVisibility->GetActiveItemUserData()->GetInt();

	CUtlStringList vecTags;
	SteamParamStringArray_t tags;
	tags.m_nNumStrings = 0;

	for ( int i = g_nTagsGridRowCount; i--; )
	{
		for ( int j = g_nTagsGridColumnCount; j--; )
		{
			CheckButton *pCheckBox = (CheckButton*)m_pItemPublishDialog->m_pTagsGrid->GetElement( i, j );
			if ( pCheckBox && pCheckBox->IsSelected() )
			{
				tags.m_nNumStrings++;
				vecTags.CopyAndAddToTail( pCheckBox->GetName() );
			}
		}
	}

	UGCUpdateHandle_t item = m_hItemUpdate = steamapicontext->SteamUGC()->StartItemUpdate( steamapicontext->SteamUtils()->GetAppID(), id );

	if ( pszTitle[0] )
		Verify( steamapicontext->SteamUGC()->SetItemTitle( item, pszTitle ) );

	if ( pszDesc[0] )
		Verify( steamapicontext->SteamUGC()->SetItemDescription( item, pszDesc ) );

	if ( pszContent[0] )
		Verify( steamapicontext->SteamUGC()->SetItemContent( item, pszContent ) );

	if ( pszPreview[0] )
		Verify( steamapicontext->SteamUGC()->SetItemPreview( item, pszPreview ) );

	if ( tags.m_nNumStrings )
	{
		tags.m_ppStrings = const_cast< const char ** >( vecTags.Base() );
		if ( !steamapicontext->SteamUGC()->SetItemTags( item, &tags ) )
		{
			Warning( "Failed to set item tags.\n" );
		}
	}

	steamapicontext->SteamUGC()->SetItemVisibility( item, (ERemoteStoragePublishedFileVisibility)visibility );

	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUGC()->SubmitItemUpdate( item, pszChanges );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::SubmitItemUpdate() Steam API call failed\n" );
		return;
	}

	m_SubmitItemUpdateResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnSubmitItemUpdateResult );
}

void CWorkshopPublishDialog::OnSubmitItemUpdateResult( SubmitItemUpdateResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnSubmitItemUpdateResult() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );
	}
	else
	{
		if ( pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement )
		{
			Warning( "User needs to accept workshop legal agreement\n" );
		}

		// Open the page in Steam
		system()->ShellExecute( "open", CFmtStrN< 64 >( "steam://url/CommunityFilePage/%llu", m_pItemPublishDialog->m_item ) );
	}

	m_hItemUpdate = k_UGCUpdateHandleInvalid;

	if ( m_pItemPublishDialog )
	{
		if ( m_pItemPublishDialog->m_pProgressUpload )
		{
			m_pItemPublishDialog->m_pProgressUpload->Close();
		}

		m_pItemPublishDialog->Close();
		m_pItemPublishDialog = NULL;
	}

	MoveToFront();

	Refresh();
}

void CWorkshopPublishDialog::DeleteItem( PublishedFileId_t id )
{
#ifdef _DEBUG
	// Warn if the item is not in the list
	{
		PublishedFileId_t inID = 0;
		for ( int i = m_pList->FirstItem(); i != m_pList->InvalidItemID(); i = m_pList->NextItem(i) )
		{
			if ( id == m_pList->GetItemData(i)->kv->GetUint64("id") )
			{
				inID = id;
				break;
			}
		}

		if ( !inID && id )
		{
			DevWarning( "Deleting file from outside of the list %llu\n", id );
		}
	}
#endif

	if ( !id )
	{
		Warning( "CWorkshopPublishDialog::DeleteItem() Invalid item\n" );
		return;
	}

	// NOTE: ISteamRemoteStorage::DeletePublishedFile was deprecated and ISteamUGC::DeleteItem was added in Steamworks SDK 141
	SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->DeletePublishedFile( id );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::DeleteItem() Steam API call failed\n" );
		return;
	}

	m_DeleteItemResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnDeleteItemResult );

	m_pDelete->SetEnabled( false );
}

void CWorkshopPublishDialog::OnDeleteItemResult( RemoteStorageDeletePublishedFileResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnDeleteItemResult() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );

		// It was deleted elsewhere, refresh
		if ( pResult->m_eResult == k_EResultFileNotFound )
		{
			Refresh();
		}
	}
	else
	{
		Msg( "Deleted item %llu\n", pResult->m_nPublishedFileId );

		// Remove deleted item from the list
		for ( int i = m_pList->FirstItem(); i != m_pList->InvalidItemID(); i = m_pList->NextItem(i) )
		{
			if ( pResult->m_nPublishedFileId == m_pList->GetItemData(i)->kv->GetUint64("id") )
			{
				m_pList->RemoveItem(i);
				break;
			}
		}
	}

	m_pDelete->SetEnabled( true );
}


CItemPublishDialog::CItemPublishDialog( Panel *pParent ) :
	BaseClass( pParent, "ItemDialog", false, false ),
	m_item( 0 )
{
	SetVisible( true );
	SetTitle( "#WorkshopMgr_ItemPublish", false );
	SetDeleteSelfOnClose( true );
	SetMoveable( true );
	SetSizeable( false );

	MakePopup();
	DoModal();
	RequestFocus();

	// NOTE: Max char count in TextEntry is not memory size, a char and wchar_t are treated equally as a 'character'.

	m_pTitleLabel = new Label( this, NULL, "#WorkshopMgr_Label_Title" );
	m_pTitleInput = new TextEntry( this, "title" );
	m_pTitleInput->SetMaximumCharCount( k_cchPublishedDocumentTitleMax - sizeof(wchar_t) );
	m_pTitleInput->SetMultiline( false );
	m_pTitleInput->SetAllowNonAsciiCharacters( true );

	m_pDescLabel = new Label( this, NULL, "#WorkshopMgr_Label_Description" );
	m_pDescInput = new TextEntry( this, "desc" );
	m_pDescInput->SetMaximumCharCount( k_cchPublishedDocumentDescriptionMax - sizeof(wchar_t) );
	m_pDescInput->SetMultiline( true );
	m_pDescInput->SetVerticalScrollbar( true );
	m_pDescInput->SetCatchEnterKey( true );
	m_pDescInput->SetAllowNonAsciiCharacters( true );

	m_pChangesLabel = new Label( this, NULL, "#WorkshopMgr_Label_UpdateDesc" );
	m_pChangesInput = new TextEntry( this, "changes" );
	m_pChangesInput->SetMaximumCharCount( k_cchPublishedDocumentChangeDescriptionMax - sizeof(wchar_t) );
	m_pChangesInput->SetMultiline( true );
	m_pChangesInput->SetVerticalScrollbar( true );
	m_pChangesInput->SetCatchEnterKey( true );
	m_pChangesInput->SetAllowNonAsciiCharacters( true );

	m_pPreviewLabel = new Label( this, NULL, "#WorkshopMgr_Label_PreviewImage" );
	m_pPreviewInput = new TextEntry( this, "previewinput" );
	m_pPreviewInput->SetMaximumCharCount( MAX_PATH - 1 );
	m_pPreviewInput->SetMultiline( false );

	m_pContentLabel = new Label( this, NULL, "#WorkshopMgr_Label_AddonContent" );
	m_pContentInput = new TextEntry( this, "content" );
	m_pContentInput->SetMaximumCharCount( MAX_PATH - 1 );
	m_pContentInput->SetMultiline( false );
#if 0
	m_pContentSize = new Label( this, NULL, "" );
#endif
	m_pVisibilityLabel = new Label( this, NULL, "#WorkshopMgr_Label_Visibility" );
	m_pVisibility = new ComboBox( this, "visibility", 4, false );
	// NOTE: Add in the order of the raw values. This is read as int in UpdateFields
	m_pVisibility->AddItem( "#Workshop_Public", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityPublic ) );
	m_pVisibility->AddItem( "#Workshop_FriendsOnly", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityFriendsOnly ) );
	m_pVisibility->AddItem( "#Workshop_Private", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityPrivate ) );
	m_pVisibility->AddItem( "#Workshop_Unlisted", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityUnlisted_149 ) );
	m_pVisibility->SilentActivateItem( k_ERemoteStoragePublishedFileVisibilityPrivate );

	m_pPreview = new CPreviewImage( this, "PreviewImage" );

	m_pPreviewBrowse = new Button( this, "PreviewBrowse", "#WorkshopMgr_ButtonSearch", this, "PreviewBrowse" );
	m_pContentBrowse = new Button( this, "ContentBrowse", "#WorkshopMgr_ButtonSearch", this, "ContentBrowse" );

	m_pClose = new Button( this, "Cancel", "#vgui_Cancel", this, "Close" );
	m_pPublish = new Button( this, "Publish", "#WorkshopMgr_ButtonPublish", this, "Publish" );

	m_pPreviewBrowse->SetContentAlignment( Label::Alignment::a_center );
	m_pContentBrowse->SetContentAlignment( Label::Alignment::a_center );
	m_pClose->SetContentAlignment( Label::Alignment::a_center );
	m_pPublish->SetContentAlignment( Label::Alignment::a_center );

	m_pTagsLabel = new Label( this, NULL, "#WorkshopMgr_Label_Tags" );

	// Using a grid lets the user define all tags in a single place and have the checkboxes auto-position.
	m_pTagsGrid = new CSimpleGrid( this, NULL );
	m_pTagsGrid->SetDimensions( g_nTagsGridRowCount, g_nTagsGridColumnCount );
	m_pTagsGrid->SetElementSize( 100, 18 );
	m_pTagsGrid->SetCellSize( 100, 16 );

	for ( int i = g_nTagsGridRowCount; i--; )
	{
		for ( int j = g_nTagsGridColumnCount; j--; )
		{
			const char *tag = g_ppWorkshopTags[i][j];
			if ( tag )
			{
#ifdef _DEBUG
				if ( V_strlen(tag) >= 256 )
					Warning( "Tag is too long (%s)\n", tag );

				for ( const char *c = tag; *c; ++c )
				{
					if ( !V_isprint(*c) )
					{
						Warning( "Tag is invalid (%s)\n", tag );
						break;
					}
				}
#endif
				m_pTagsGrid->SetElement( i, j, new CheckButton( NULL, tag, tag ) );
			}
		}
	}
}

void CItemPublishDialog::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder("MenuBorder") );
}

void CItemPublishDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( 640, 480 );
	MoveToCenterOfScreen();

	m_pTitleLabel->SetPos( 346, 26 );
	m_pTitleLabel->SetSize( 250, 24 );

	m_pTitleInput->SetPos( 346, 50 );
	m_pTitleInput->SetSize( 270, 24 );

	m_pDescLabel->SetPos( 346, 74 );
	m_pDescLabel->SetSize( 270, 24 );

	m_pDescInput->SetPos( 346, 98 );
	m_pDescInput->SetSize( 270, 102 );

	m_pChangesLabel->SetPos( 346, 200 );
	m_pChangesLabel->SetSize( 270, 24 );

	m_pChangesInput->SetPos( 346, 224 );
	m_pChangesInput->SetSize( 270, 88 );

	m_pPreviewLabel->SetPos( 26, 306 );
	m_pPreviewLabel->SetSize( 270, 24 );

	m_pPreviewInput->SetPos( 26, 330 );
	m_pPreviewInput->SetSize( 510, 24 );

	m_pPreview->SetPos( 24, 34 );
	m_pPreview->SetSize( 295, 166 );

	m_pPreviewBrowse->SetPos( 540, 330 );
	m_pPreviewBrowse->SetSize( 76, 24 );
#if 0
	int w, t;
	m_pContentLabel->GetContentSize( w, t );
	m_pContentSize->SetPos( w + 16, 354 );
	m_pContentSize->SetSize( 270, 24 );
#endif
	m_pContentLabel->SetPos( 26, 354 );
	m_pContentLabel->SetSize( 270, 24 );

	m_pContentInput->SetPos( 26, 378 );
	m_pContentInput->SetSize( 510, 24 );

	m_pContentBrowse->SetPos( 540, 378 );
	m_pContentBrowse->SetSize( 76, 24 );

	m_pVisibilityLabel->SetPos( 26, 402 );
	m_pVisibilityLabel->SetSize( 100, 24 );

	m_pVisibility->SetPos( 26, 426 );
	m_pVisibility->SetSize( 100, 24 );

	m_pClose->SetPos( 466, 466 - 20 );
	m_pClose->SetSize( 150, 24 );

	m_pPublish->SetPos( 312, 466 - 20 );
	m_pPublish->SetSize( 150, 24 );

	m_pTagsLabel->SetPos( 26, 200 );
	m_pTagsLabel->SetSize( 85, 24 );

	m_pTagsGrid->SetPos( 20, 221 );
	m_pTagsGrid->SetSize( 100 * g_nTagsGridColumnCount, 16 * (g_nTagsGridRowCount-1) + 18 ); // height: rows-1+element
	m_pTagsGrid->LayoutElements();
}

void CItemPublishDialog::OnClose()
{
	Assert( g_pWorkshopPublish );

	g_pWorkshopPublish->MoveToFront();
	g_pWorkshopPublish->m_pItemPublishDialog = NULL;

	BaseClass::OnClose();
}

void CItemPublishDialog::OnCommand( const char *command )
{
	if ( !V_stricmp( "Publish", command ) )
	{
		OnPublish();
	}
	else if ( !V_stricmp( "PreviewBrowse", command ) )
	{
		FileOpenDialog *pFileBrowser = new FileOpenDialog( this, "Browse preview image", FileOpenDialogType_t::FOD_OPEN );
		pFileBrowser->AddFilter( "*.jpg;*.jpeg;*.png;*.gif;*.bmp", "Image files (*.jpg;*.jpeg;*.png;*.gif;*.bmp)", true );

		pFileBrowser->AddActionSignalTarget( this );
		pFileBrowser->SetDeleteSelfOnClose( true );
		pFileBrowser->SetVisible( true );
		pFileBrowser->DoModal();
	}
	else if ( !V_stricmp( "ContentBrowse", command ) )
	{
		// NOTE: DirectorySelectDialog sucks, but FileOpenDialogType_t::FOD_SELECT_DIRECTORY does not work
		DirectorySelectDialog *pDirectoryBrowser = new DirectorySelectDialog( this, "Browse content directory" );
		pDirectoryBrowser->MakeReadyForUse();
		pDirectoryBrowser->AddActionSignalTarget( this );
		pDirectoryBrowser->SetDeleteSelfOnClose( true );
		pDirectoryBrowser->SetVisible( true );
		pDirectoryBrowser->DoModal();

		char pLocalPath[255];
		g_pFullFileSystem->GetCurrentDirectory( pLocalPath, 255 );
		pDirectoryBrowser->SetStartDirectory( pLocalPath );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

void CItemPublishDialog::OnPublish()
{
	// Steam doesn't strip whitespace (but web interface does), make sure text is sent stripped.
	bool bStripped;

	char pszPreview[ MAX_PATH ];
	m_pPreviewInput->GetText( pszPreview, sizeof(pszPreview) );
	bStripped = Q_StripPrecedingAndTrailingWhitespace( pszPreview );

	// Require preview image for initial commit
	if ( !m_item && !pszPreview[0] )
	{
		Warning( "Preview image field empty\n" );
		m_pPreviewInput->SetText( L"" );
		m_pPreviewInput->RequestFocus();
		return;
	}
	else if ( bStripped )
	{
		m_pPreviewInput->SetText( pszPreview );
	}

	// Require valid image
	if ( pszPreview[0] && !g_pFullFileSystem->FileExists( pszPreview ) )
	{
		Warning( "Preview image does not exist\n" );
		m_pPreviewInput->SelectAllText( true );
		m_pPreviewInput->RequestFocus();
		return;
	}

	char pszContent[ MAX_PATH ];
	m_pContentInput->GetText( pszContent, sizeof(pszContent) );
	bStripped = Q_StripPrecedingAndTrailingWhitespace( pszContent );

	// Require content for initial commit
	if ( !m_item && !pszContent[0] )
	{
		Warning( "Content field empty\n" );
		m_pContentInput->SetText( L"" );
		m_pContentInput->RequestFocus();
		return;
	}
	else if ( bStripped )
	{
		m_pContentInput->SetText( pszContent );
	}

	// Require valid content
	if ( pszContent[0] && !g_pFullFileSystem->IsDirectory( pszContent ) && !g_pFullFileSystem->FileExists( pszContent ) )
	{
		Warning( "Content does not exist\n" );
		m_pContentInput->SelectAllText( true );
		m_pContentInput->RequestFocus();
		return;
	}

	char pszTitle[ k_cchPublishedDocumentTitleMax - sizeof(wchar_t) ];
	m_pTitleInput->GetText( pszTitle, sizeof(pszTitle) );
	bStripped = Q_AggressiveStripPrecedingAndTrailingWhitespace( pszTitle );

	// Require title
	if ( !pszTitle[0] )
	{
		Warning( "Title field empty\n" );
		m_pTitleInput->SetText( L"" );
		m_pTitleInput->RequestFocus();
		return;
	}
	else if ( bStripped )
	{
		m_pTitleInput->SetText( pszTitle );
	}

	wchar_t pszDesc[ 1024 ];
	m_pDescInput->GetText( pszDesc, sizeof(pszDesc) );
	Q_AggressiveStripPrecedingAndTrailingWhitespaceW( pszDesc );

	// Require description for initial commit
	if ( !m_item && !pszDesc[0] )
	{
		Warning( "Description field empty\n" );
		m_pDescInput->SetText( L"" );
		m_pDescInput->RequestFocus();
		return;
	}

	wchar_t pszChanges[ 1024 ];
	m_pChangesInput->GetText( pszChanges, sizeof(pszChanges) );
	Q_AggressiveStripPrecedingAndTrailingWhitespaceW( pszChanges );

	// If content is updated, request change notes once. (Allow initial commit without message)
	// This is also required in L4D2 and HLVR workshop tools.
	if ( m_item && pszContent[0] && !pszChanges[0] )
	{
		Warning( "Change notes field empty\n" );
		m_pChangesInput->SetText( L"Undocumented changes" );
		m_pChangesInput->SelectAllText( true );
		m_pChangesInput->RequestFocus();
		return;
	}

	if ( m_item )
	{
		g_pWorkshopPublish->SubmitItemUpdate( m_item );
	}
	else
	{
		g_pWorkshopPublish->CreateItem();
	}

	if ( m_pProgressUpload )
		m_pProgressUpload->MarkForDeletion();

	m_pProgressUpload = new CUploadProgressBox( this );
}
#if 0
void CItemPublishDialog::SetAddonContent( const char *filename )
{
	unsigned int size = g_pFullFileSystem->Size( filename );
	if ( size )
	{
		m_pContentSize->SetText( V_pretifymem( size, 2, true ) );
	}
	else
	{
		m_pContentSize->SetText( "" );
	}
}
#endif
void CItemPublishDialog::SetPreviewImage( const char *filename )
{
	const char *ext = V_GetFileExtension( filename );

	if ( !ext )
	{
		m_pPreview->RemoveImage();
	}
	else if ( !V_stricmp( ext, "jpg" ) || !V_stricmp( ext, "jpeg" ) )
	{
		m_pPreview->SetJPEGImage( filename );
	}
	else if ( !V_stricmp( ext, "png" ) )
	{
		m_pPreview->SetPNGImage( filename );
	}
	else
	{
		m_pPreview->RemoveImage();

		// if this is a type that cannot be loaded, always draw "loading" message
		if ( g_pFullFileSystem->FileExists( filename ) )
		{
			if ( !V_stricmp( ext, "gif" ) || !V_stricmp( ext, "bmp" ) )
			{
				m_pPreview->m_bLoading = true;
			}
		}
	}
}

void CItemPublishDialog::SetPreviewImage( CUtlBuffer &file, const char *filename )
{
	const char *ext = V_GetFileExtension( filename );

	if ( !ext )
	{
		m_pPreview->RemoveImage();
	}
	else if ( !V_stricmp( ext, "jpg" ) || !V_stricmp( ext, "jpeg" ) )
	{
		m_pPreview->SetJPEGImage( file );
	}
	else if ( !V_stricmp( ext, "png" ) )
	{
		m_pPreview->SetPNGImage( file );
	}
	else
	{
		m_pPreview->RemoveImage();

		// if this is a type that cannot be loaded, always draw "loading" message
		m_pPreview->m_bLoading = true;
	}
}

void CItemPublishDialog::OnTextChanged( Panel *panel )
{
	if ( panel == m_pPreviewInput )
	{
		// Try to load image from current text
		if ( m_pPreviewInput->GetTextLength() )
		{
			char path[ MAX_PATH ];
			m_pPreviewInput->GetText( path, sizeof(path) );
			SetPreviewImage( path );
		}
		else
		{
			m_pPreview->RemoveImage();
		}
	}
#if 0
	else if ( panel == m_pContentInput )
	{
		// Get content size
		if ( m_pContentInput->GetTextLength() )
		{
			char path[ MAX_PATH ];
			m_pContentInput->GetText( path, sizeof(path) );
			SetAddonContent( path );
		}
		else
		{
			m_pContentSize->SetText( "" );
		}
	}
#endif
}

void CItemPublishDialog::OnFileSelected( const char *fullpath )
{
	m_pPreviewInput->SetText( fullpath );
	SetPreviewImage( fullpath );

	// Set content to the same path as preview image if it was not already set
	if ( !m_pContentInput->GetTextLength() )
	{
		char path[MAX_PATH];
		V_StripExtension( fullpath, path, MAX_PATH );
		m_pContentInput->SetText( path );
	}
}

void CItemPublishDialog::OnDirectorySelected( const char *dir )
{
	m_pContentInput->SetText( dir );

	// Set preview image to the same path as content if it was not already set
	if ( !m_pPreviewInput->GetTextLength() )
	{
		char path[MAX_PATH];
		int len = V_strlen(dir);

		if ( dir[len-1] == CORRECT_PATH_SEPARATOR || dir[len-1] == INCORRECT_PATH_SEPARATOR )
		{
			// ignore trailing slash
			V_strncpy( path, dir, len );
		}
		else
		{
			V_strncpy( path, dir, sizeof(path) );
		}

		V_strcat( path, ".jpg", sizeof(path) );
		m_pPreviewInput->SetText( path );

		// Load image if it exists
		if ( g_pFullFileSystem->FileExists( path ) )
		{
			m_pPreview->SetJPEGImage( path );
		}
	}
}

void CItemPublishDialog::UpdateFields( KeyValues *data )
{
	m_item = data->GetUint64("id");

	wchar_t wtitle[128];
	V_snwprintf( wtitle, sizeof(wtitle), L"%ls (%llu)", g_pVGuiLocalize->Find("#WorkshopMgr_ItemPublish"), m_item );
	SetTitle( wtitle, false );
	m_pPublish->SetText( "#WorkshopMgr_ButtonUpdate" );

	m_pTitleInput->SetText( data->GetString("title") );
	m_pDescInput->SetText( data->GetString("description") );
	m_pVisibility->SilentActivateItem( data->GetInt("visibility_raw") );

	const char *ppTags = data->GetString("tags");
	if ( ppTags[0] )
	{
		CUtlStringList tags;
		V_SplitString( ppTags, ",", tags );

		FOR_EACH_VEC( tags, i )
		{
			const char *tag = tags[i];
			CheckButton *tagCheckBox = (CheckButton*)m_pTagsGrid->FindChildByName( tag );

			// Tag from the workshop item is not found in g_ppWorkshopTags
			AssertMsg( tagCheckBox, "Tag '%s' not found", tag );

			if ( tagCheckBox )
			{
				Assert( !V_stricmp( "CheckButton", tagCheckBox->GetClassName() ) );
				tagCheckBox->SetSelected( true );
			}
		}
	}
}


CModalMessageBox::CModalMessageBox( Panel *pParent, const char *msg ) :
	BaseClass( pParent, NULL, false, false )
{
	SetVisible( true );
	SetTitle( L"", false );
	SetDeleteSelfOnClose( true );
	SetCloseButtonVisible( false );
	SetMoveable( false );
	SetSizeable( false );

	MakePopup();
	DoModal();
	MoveToFront();

	SetKeyBoardInputEnabled( false );

	m_pLabel = new Label( this, NULL, msg );
}

void CModalMessageBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder("MenuBorder") );
}

void CModalMessageBox::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( 384, 128 );
	MoveToCenterOfScreen();

	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;

	int leftEdge = x + 16;

	m_pLabel->SetSize( wide - 44, 24 );
	m_pLabel->SetPos( leftEdge, y + 12 );
}


CUploadProgressBox::CUploadProgressBox( Panel *pParent ) :
	BaseClass( pParent, "ProgressBox", false, false ),
	m_status( (EItemUpdateStatus)~0 )
{
	SetVisible( true );
	SetTitle( "#WorkshopMgr_Status_Publishing", false );
	SetDeleteSelfOnClose( true );
	SetCloseButtonVisible( false );
	SetMoveable( false );
	SetSizeable( false );
	// update twice a second
	ivgui()->AddTickSignal( GetVPanel(), 500 );

	MakePopup();
	DoModal();
	MoveToFront();

	SetKeyBoardInputEnabled( false );

	m_pLabel = new Label( this, NULL, "#WorkshopMgr_Status_Creating" );
	m_pProgressBar = new ProgressBar( this, NULL );
}

void CUploadProgressBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder("MenuBorder") );

	m_pProgressBar->SetFgColor( pScheme->GetColor( "ProgressBar.FgColor", Color(216, 222, 211, 255) ) );
}

void CUploadProgressBox::PerformLayout()
{
	BaseClass::PerformLayout();

	SetSize( 384, 128 );
	MoveToCenterOfScreen();

	int x, y, wide, tall;
	GetClientArea( x, y, wide, tall );
	wide += x;
	tall += y;

	int leftEdge = x + 16;

	m_pLabel->SetSize( wide - 44, 24 );
	m_pLabel->SetPos( leftEdge, y + 12 );

	m_pProgressBar->SetPos( leftEdge, y + 14 + m_pLabel->GetTall() + 2 );
	m_pProgressBar->SetSize( wide - 44, 24 );
}

void CUploadProgressBox::OnTick()
{
	if ( g_pWorkshopPublish->GetItemUpdate() == k_UGCUpdateHandleInvalid )
	{
		return;
	}

	uint64 punBytesProcessed, punBytesTotal;
	EItemUpdateStatus status = steamapicontext->SteamUGC()->GetItemUpdateProgress( g_pWorkshopPublish->GetItemUpdate(), &punBytesProcessed, &punBytesTotal );

	if ( punBytesTotal )
	{
		m_pProgressBar->SetProgress( (float)punBytesProcessed / (float)punBytesTotal );
	}
	else
	{
		m_pProgressBar->SetProgress( 1.0f );
	}

	if ( m_status != status )
	{
		m_status = status;
		switch ( status )
		{
		case k_EItemUpdateStatusPreparingConfig:
			m_pLabel->SetText( "#WorkshopMgr_Status_PreparingConfig" );
			break;
		case k_EItemUpdateStatusPreparingContent:
			m_pLabel->SetText( "#WorkshopMgr_Status_PreparingContent" );
			break;
		case k_EItemUpdateStatusUploadingContent:
			m_pLabel->SetText( "#WorkshopMgr_Status_UploadingContent" );
			break;
		case k_EItemUpdateStatusUploadingPreviewFile:
			m_pLabel->SetText( "#WorkshopMgr_Status_UploadingPreviewFile" );
			break;
		case k_EItemUpdateStatusCommittingChanges:
			m_pLabel->SetText( "#WorkshopMgr_Status_CommittingChanges" );
			break;
		case k_EItemUpdateStatusInvalid:
			Close();
			break;
		}
	}
}


CDeleteMessageBox::CDeleteMessageBox( Panel *pParent, const wchar_t *title, const wchar_t *msg ) :
	BaseClass( title, msg, pParent )
{
	SetCloseButtonVisible( true );
	SetDeleteSelfOnClose( true );
	SetSizeable( false );
	SetMoveable( false );

	MakePopup();
	DoModal();
	MoveToFront();

	m_pCancelButton->SetText( "#vgui_Cancel" );
	m_pOkButton->SetText( "#vgui_ok" );

	m_pCancelButton->SetVisible( true );
	m_pOkButton->SetContentAlignment( Label::Alignment::a_center );
	m_pCancelButton->SetContentAlignment( Label::Alignment::a_center );

	// Activate OK button 1 second later
	ivgui()->AddTickSignal( GetVPanel(), 1000 );
	m_pOkButton->SetEnabled( false );
}

void CDeleteMessageBox::OnTick()
{
	ivgui()->RemoveTickSignal( GetVPanel() );

	m_pOkButton->SetEnabled( true );
	m_pCancelButton->RequestFocus();
}

void CDeleteMessageBox::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetBorder( pScheme->GetBorder("MenuBorder") );

	Color dark( 160, 0, 0, 255 );
	Color bright( 200, 0, 0, 255 );

	m_pOkButton->SetDefaultColor( m_pOkButton->GetButtonDefaultFgColor(), dark );
	m_pOkButton->SetSelectedColor( m_pOkButton->GetButtonSelectedFgColor(), dark );
	m_pOkButton->SetDepressedColor( m_pOkButton->GetButtonDepressedFgColor(), bright );
	m_pOkButton->SetArmedColor( m_pOkButton->GetButtonArmedFgColor(), bright );
}


CSimpleGrid::CSimpleGrid( Panel *pParent, const char *name ) :
	BaseClass( pParent, name ),
	m_rows(0),
	m_columns(0),
	m_panelWide(0),
	m_panelTall(0),
	m_cellWide(0),
	m_cellTall(0)
{
}

void CSimpleGrid::LayoutElements()
{
	for ( int row = 0; row < m_rows; ++row )
	{
		for ( int col = 0; col < m_columns; ++col )
		{
			Assert( m_Elements.IsValidIndex( m_columns * row + col ) );

			Panel *panel = m_Elements[ m_columns * row + col ];
			if ( !panel )
				continue;

			panel->SetPos( col * m_cellWide, row * m_cellTall );
			panel->SetSize( m_panelWide, m_panelTall );
		}
	}
}

void CSimpleGrid::SetDimensions( int row, int col )
{
	Assert( row >= 0 && col >= 0 );

	int oldSize = m_rows * m_columns;
	int newSize = row * col;

	if ( newSize > oldSize )
	{
		m_Elements.EnsureCount( newSize );

		for ( int i = oldSize; i < newSize; ++i )
			m_Elements[i] = NULL;
	}
	else
	{
		for ( int i = oldSize - 1; i >= newSize; --i )
		{
			Panel *elem = m_Elements[i];

			if ( elem )
			{
				elem->MarkForDeletion();
			}

			m_Elements.FastRemove( i );
		}
	}

	m_rows = row;
	m_columns = col;
}

void CSimpleGrid::SetElementSize( int w, int t )
{
	m_panelWide = w;
	m_panelTall = t;
}

void CSimpleGrid::SetCellSize( int w, int t )
{
	m_cellWide = w;
	m_cellTall = t;
}

void CSimpleGrid::SetElement( int row, int col, Panel *panel )
{
	Assert( m_Elements.IsValidIndex( m_columns * row + col ) );

	int idx = m_columns * row + col;

	if ( panel )
	{
		m_Elements[idx] = panel;
		panel->SetParent( GetVPanel() );
	}
	else
	{
		Panel *elem = m_Elements[idx];
		if ( elem )
		{
			elem->MarkForDeletion();
			m_Elements[idx] = NULL;
		}
	}
}

Panel *CSimpleGrid::GetElement( int row, int col )
{
	Assert( m_Elements.IsValidIndex( m_columns * row + col ) );
	return m_Elements[ m_columns * row + col ];
}




CON_COMMAND( workshop_publish, "" )
{
	if ( !steamapicontext || !ClientSteamContext().BLoggedOn() ||
		!steamapicontext->SteamUtils() || !steamapicontext->SteamUGC() || !steamapicontext->SteamRemoteStorage() )
	{
		Warning("steamapicontext == NULL\n");
		return;
	}

	if ( g_pWorkshopPublish )
	{
		g_pWorkshopPublish->MoveToCenterOfScreen();
		g_pWorkshopPublish->MoveToFront();
		g_pWorkshopPublish->RequestFocus();
		return;
	}

	new CWorkshopPublishDialog();
}
