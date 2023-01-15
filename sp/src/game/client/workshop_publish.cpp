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


// To remember what was selected before
enum SelectedFileFilter
{
	FILE_FILTER_JPG = 0, // default
	FILE_FILTER_PNG,
	FILE_FILTER_GIF
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
	void Refresh();
	void CreateItem();
	void SubmitItemUpdate( PublishedFileId_t );
	void DeleteItem( PublishedFileId_t );

	UGCUpdateHandle_t GetItemUpdate() const { return m_hItemUpdate; }

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

	long m_nQueryTime;
	int m_nLastUsedFileFilter;

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

	FileOpenDialog *m_pFileBrowser;
	DirectorySelectDialog *m_pDirectoryBrowser;

	DHANDLE< CUploadProgressBox > m_pProgressUpload;

	bool m_bDownloadedTempFiles;
};


class CPreviewImage : public Panel
{
public:
	CPreviewImage( Panel* pParent, const char *name ) : Panel( pParent, name ),
		m_iTexture(-1),
		m_nDrawWidth(0),
		m_nDrawHeight(0),
		m_s1(1.0f),
		m_t1(1.0f)
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
			surface()->DrawSetColor( 0, 0, 0, 255 );
			// Panel size was increased to see user images better, draw the empty box a bit smaller
			surface()->DrawFilledRect( 6, 20, GetWide()-30, GetTall() );
		}
	}

	void SetJPEGImage( const char *filename )
	{
#ifdef JPEG_LIB_VERSION
		bool JPEGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );

		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( JPEGtoRGBA( filename, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded jpeg %dx%d '%s'\n", imageWidth, imageHeight, V_GetFileName( filename ) );
		}
		else
		{
			if ( m_iTexture > 0 )
				surface()->DestroyTextureID( m_iTexture );
			m_iTexture = -1;
		}
#endif
	}

	void SetPNGImage( const char *filename )
	{
#ifdef PNG_LIBPNG_VER
		bool PNGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );

		int imageWidth, imageHeight;
		CUtlMemory< byte > image;

		if ( PNGtoRGBA( filename, image, imageWidth, imageHeight ) )
		{
			SetImageRGBA( image.Base(), imageWidth, imageHeight );
			DevMsg( "Loaded png %dx%d '%s'\n", imageWidth, imageHeight, V_GetFileName( filename ) );
		}
		else
		{
			if ( m_iTexture > 0 )
				surface()->DestroyTextureID( m_iTexture );
			m_iTexture = -1;
		}
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
			if ( imageWidth > panelWidth )
			{
				m_nDrawWidth = panelWidth;
			}
			else
			{
				m_nDrawWidth = imageWidth;
			}

			m_nDrawHeight = imageHeight * m_nDrawWidth / imageWidth;
		}
		else
		{
			// Fit to height
			if ( imageHeight > panelHeight )
			{
				m_nDrawHeight = panelHeight;
			}
			else
			{
				m_nDrawHeight = imageHeight;
			}

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
	CDeleteMessageBox( Panel *pParent, const char *title, const char *msg );
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
bool JPEGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );
bool JPEGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height );

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
		src->m_buffer->SeekGet( CUtlBuffer::SEEK_CURRENT, num_bytes );

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

		m_buffer = pBuffer;
	}

	CUtlBuffer *m_buffer;
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

bool JPEGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height )
{
	//
	// See libjpeg.txt for details of this code
	//
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
bool PNGtoRGBA( const char *filename, CUtlMemory< byte > &buffer, int &width, int &height );
bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height );

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
		return false;
	}

	buffer.Init( 0, rowbytes * image_height );

	byte *image_data = buffer.Base();
	row_pointers = (png_bytepp)malloc( image_height * sizeof(png_bytep) );

	if ( !image_data || !row_pointers )
	{
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		if ( row_pointers )
			free( row_pointers );
		return false;
	}

	for ( png_uint_32 i = 0; i < image_height; ++i )
		row_pointers[i] = image_data + i*rowbytes;

	png_read_image( png_ptr, row_pointers );
	//png_read_end( png_ptr, NULL );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	free( row_pointers );

	return true;
}
#endif




int __cdecl SortTitle( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return V_stricmp( item1.kv->GetName(), item2.kv->GetName() );
}

int __cdecl SortTimeUpdated( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return item1.kv->GetInt("timeupdated_raw") - item2.kv->GetInt("timeupdated_raw");
}

int __cdecl SortTimeCreated( ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return item1.kv->GetInt("timecreated_raw") - item2.kv->GetInt("timecreated_raw");
}

void GetPreviewImageTempLocation( PublishedFileId_t fileId, char *out, int size )
{
	char path[MAX_PATH];
	Verify( g_pFullFileSystem->GetCurrentDirectory( path, sizeof(path) ) );

	V_snprintf( out, size, "%s%cpreviewfile_%llu.jpg", path, CORRECT_PATH_SEPARATOR, fileId );
}



CWorkshopPublishDialog::CWorkshopPublishDialog() :
	BaseClass( NULL, "WorkshopPublishDialog", false, false ),
	m_pItemPublishDialog( NULL ),
	m_nItemToDelete( 0 ),
	m_hItemUpdate( k_UGCUpdateHandleInvalid ),
	m_nLastUsedFileFilter( FILE_FILTER_JPG )
{
	Assert( !g_pWorkshopPublish );
	g_pWorkshopPublish = this;

	SetParent( enginevgui->GetPanel(PANEL_TOOLS) );
	SetVisible( true );
	SetTitle( "Browse Published Files", false );
	SetDeleteSelfOnClose( true );
	SetMoveable( true );
	SetSizeable( false );

	MakePopup();
	RequestFocus();

	m_pList = new ListPanel( this, "PublishedFileList" );
	m_pList->SetMultiselectEnabled( false );
	m_pList->SetEmptyListText( "No published files" );
	m_pList->AddColumnHeader( 0, "title", "Title", 30, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 1, "timeupdated", "Last Updated", 44, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 2, "timecreated", "Date Created", 44, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 3, "visibility", "Visibility", 20, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );
	m_pList->AddColumnHeader( 4, "id", "ID", 24, ListPanel::ColumnFlags_e::COLUMN_RESIZEWITHWINDOW );

	m_pList->SetSortFunc( 0, &SortTitle );
	m_pList->SetSortFunc( 1, &SortTimeUpdated );
	m_pList->SetSortFunc( 2, &SortTimeCreated );

	m_pAdd = new Button( this, "Add", "Add", this, "Add" );
	m_pDelete = new Button( this, "Delete", "Delete", this, "Delete" );
	m_pEdit = new Button( this, "Edit", "Edit", this, "Edit" );
	m_pRefresh = new Button( this, "Refresh", "Refresh", this, "Refresh" );

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
			m_pDeleteMessageBox->Close();
			return;
		}

		m_nItemToDelete = m_pList->GetItemData(item)->kv->GetUint64("id");
		const char *title = m_pList->GetItemData(item)->kv->GetString("title");

		m_pDeleteMessageBox = new CDeleteMessageBox( this,
			CFmtStr( "Delete Item (%llu) %s", m_nItemToDelete, title ),
			"Are you sure you want to delete this item? This cannot be undone!" );

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
		char temp[MAX_PATH];
		GetPreviewImageTempLocation( details.m_nPublishedFileId, temp, sizeof(temp) );

		SteamAPICall_t hSteamAPICall = steamapicontext->SteamRemoteStorage()->UGCDownloadToLocation( details.m_hPreviewFile, temp, 1 );
		if ( hSteamAPICall == k_uAPICallInvalid )
		{
			Warning( "CWorkshopPublishDialog::QueryItem() Steam API call failed\n" );
		}
		else
		{
			m_RemoteStorageDownloadUGCResult.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult );
		}
	}
}

void CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult( RemoteStorageDownloadUGCResult_t *pResult, bool bIOFailure )
{
	Assert( !bIOFailure );

	if ( pResult->m_eResult != k_EResultOK )
	{
		Warning( "CWorkshopPublishDialog::OnRemoteStorageDownloadUGCResult() (%i) %s\n", pResult->m_eResult, GetResultDesc( pResult->m_eResult ) );
		return;
	}

	DevMsg( "Downloaded %u bytes '%s'\n", pResult->m_nSizeInBytes, pResult->m_pchFileName );

	if ( !m_pItemPublishDialog )
		return; // panel closed before download

	Assert( m_pItemPublishDialog->m_item );

	// Don't set downloaded image if preview was already set
	if ( !m_pItemPublishDialog->m_pPreviewInput->GetTextLength() )
	{
		m_pItemPublishDialog->m_bDownloadedTempFiles = true;

		char fullpath[MAX_PATH];
		GetPreviewImageTempLocation( m_pItemPublishDialog->m_item, fullpath, sizeof(fullpath) );

		m_pItemPublishDialog->m_pPreview->SetJPEGImage( fullpath );
	}
}

void CWorkshopPublishDialog::Refresh()
{
	UGCQueryHandle_t hUGCQuery = steamapicontext->SteamUGC()->CreateQueryUserUGCRequest( ClientSteamContext().GetLocalPlayerSteamID().GetAccountID(),
		k_EUserUGCList_Published,
		k_EUGCMatchingUGCType_Items,
		k_EUserUGCListSortOrder_LastUpdatedDesc,
		steamapicontext->SteamUtils()->GetAppID(),
		steamapicontext->SteamUtils()->GetAppID(),
		1 ); // TODO: Implement support for more than 50 items

	if ( hUGCQuery == k_UGCQueryHandleInvalid )
	{
		Warning( "CWorkshopPublishDialog::Refresh() k_UGCQueryHandleInvalid\n" );
		return;
	}

	SteamAPICall_t hSteamAPICall = steamapicontext->SteamUGC()->SendQueryUGCRequest( hUGCQuery );
	if ( hSteamAPICall == k_uAPICallInvalid )
	{
		Warning( "CWorkshopPublishDialog::Refresh() Steam API call failed\n" );
		steamapicontext->SteamUGC()->ReleaseQueryUGCRequest( hUGCQuery );
		return;
	}

	m_SteamUGCQueryCompleted.Set( hSteamAPICall, this, &CWorkshopPublishDialog::OnSteamUGCQueryCompleted );

	m_nQueryTime = system()->GetTimeMillis();
	m_pList->RemoveAll();
	m_pRefresh->SetEnabled( false );
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

		CFmtStr strID( "%llu", details.m_nPublishedFileId );

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

		time_t t = details.m_rtimeUpdated;
		tm *date = localtime( &t );
		data->SetString( "timeupdated", CFmtStr("%d-%02d-%02d %02d:%02d:%02d",
			date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec) );

		t = details.m_rtimeCreated;
		date = localtime( &t );
		data->SetString( "timecreated", CFmtStr("%d-%02d-%02d %02d:%02d:%02d",
			date->tm_year+1900, date->tm_mon+1, date->tm_mday, date->tm_hour, date->tm_min, date->tm_sec) );

		// for sorting
		data->SetInt( "timeupdated_raw", details.m_rtimeUpdated );
		data->SetInt( "timecreated_raw", details.m_rtimeCreated );

		switch ( (int)details.m_eVisibility )
		{
		case k_ERemoteStoragePublishedFileVisibilityPublic:
			data->SetString( "visibility", "Public" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityFriendsOnly:
			data->SetString( "visibility", "Friends Only" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityPrivate:
			data->SetString( "visibility", "Private" );
			break;
		case k_ERemoteStoragePublishedFileVisibilityUnlisted_149:
			data->SetString( "visibility", "Unlisted" );
			break;
		}

		data->SetInt( "visibility_raw", details.m_eVisibility );
		data->SetString( "tags", details.m_rgchTags );

		// NOTE: Truncated tags are not retrievable. Use ISteamUGC::GetQueryUGCTag when possible.
		Assert( !details.m_bTagsTruncated );

		m_pList->ApplyItemChanges( idx );
	}

	steamapicontext->SteamUGC()->ReleaseQueryUGCRequest( pResult->m_handle );

	Msg( "Completed query in %f seconds\n", (float)(system()->GetTimeMillis() - m_nQueryTime) / 1e3f );
	m_nQueryTime = 0;

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

	SubmitItemUpdate( pResult->m_nPublishedFileId );

	// Update labels
	m_pItemPublishDialog->m_pProgressUpload->OnTick();
}

void CWorkshopPublishDialog::SubmitItemUpdate( PublishedFileId_t id )
{
	Assert( m_pItemPublishDialog );

	char pszTitle[ k_cchPublishedDocumentTitleMax - 1 ];
	m_pItemPublishDialog->m_pTitleInput->GetText( pszTitle, sizeof(pszTitle) );

	char pszDesc[ k_cchPublishedDocumentDescriptionMax ];
	m_pItemPublishDialog->m_pDescInput->GetText( pszDesc, sizeof(pszDesc) );

	char pszContent[ MAX_PATH ];
	m_pItemPublishDialog->m_pContentInput->GetText( pszContent, sizeof(pszContent) );

	char pszPreview[ MAX_PATH ];
	m_pItemPublishDialog->m_pPreviewInput->GetText( pszPreview, sizeof(pszPreview) );

	char pszChanges[ k_cchPublishedDocumentChangeDescriptionMax ];
	m_pItemPublishDialog->m_pChangesInput->GetText( pszChanges, sizeof(pszChanges) );

	int visibility = m_pItemPublishDialog->m_pVisibility->GetActiveItemUserData()->GetInt();

	UGCUpdateHandle_t item = m_hItemUpdate = steamapicontext->SteamUGC()->StartItemUpdate( steamapicontext->SteamUtils()->GetAppID(), id );

	if ( pszTitle[0] )
		Verify( steamapicontext->SteamUGC()->SetItemTitle( item, pszTitle ) );

	if ( pszDesc[0] )
		Verify( steamapicontext->SteamUGC()->SetItemDescription( item, pszDesc ) );

	if ( pszContent[0] )
		Verify( steamapicontext->SteamUGC()->SetItemContent( item, pszContent ) );

	if ( pszPreview[0] )
		Verify( steamapicontext->SteamUGC()->SetItemPreview( item, pszPreview ) );

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
	else if ( pResult->m_bUserNeedsToAcceptWorkshopLegalAgreement )
	{
		Warning( "User needs to accept workshop legal agreement\n" );
	}

	m_hItemUpdate = k_UGCUpdateHandleInvalid;

	if ( m_pItemPublishDialog )
	{
		if ( m_pItemPublishDialog->m_pProgressUpload )
		{
			m_pItemPublishDialog->m_pProgressUpload->Close();
			m_pItemPublishDialog->m_pProgressUpload = NULL;
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
	// Only allow deletion of items in the list
	{
		PublishedFileId_t inID = id;
		id = 0;
		for ( int i = m_pList->FirstItem(); i != m_pList->InvalidItemID(); i = m_pList->NextItem(i) )
		{
			if ( inID == m_pList->GetItemData(i)->kv->GetUint64("id") )
			{
				id = inID;
				break;
			}
		}
		Assert( id );
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
	m_item( 0 ),
	m_pFileBrowser( NULL ),
	m_pDirectoryBrowser( NULL ),
	m_bDownloadedTempFiles( 0 )
{
	SetVisible( true );
	SetTitle( "Item Publish", false );
	SetDeleteSelfOnClose( true );
	SetMoveable( true );
	SetSizeable( false );

	MakePopup();
	DoModal();
	RequestFocus();

	m_pTitleLabel = new Label( this, NULL, "Title:" );
	m_pTitleInput = new TextEntry( this, "title" );
	m_pTitleInput->SetMaximumCharCount( k_cchPublishedDocumentTitleMax - 1 );
	m_pTitleInput->SetMultiline( false );

	m_pDescLabel = new Label( this, NULL, "Description:" );
	m_pDescInput = new TextEntry( this, "desc" );
	m_pDescInput->SetMaximumCharCount( k_cchPublishedDocumentDescriptionMax - 1 );
	m_pDescInput->SetMultiline( true );

	m_pChangesLabel = new Label( this, NULL, "Changes this update:" );
	m_pChangesInput = new TextEntry( this, "changes" );
	m_pChangesInput->SetMaximumCharCount( k_cchPublishedDocumentChangeDescriptionMax - 1 );
	m_pChangesInput->SetMultiline( true );

	m_pPreviewLabel = new Label( this, NULL, "Preview Image:" );
	m_pPreviewInput = new TextEntry( this, "previewinput" );
	m_pPreviewInput->SetMaximumCharCount( MAX_PATH - 1 );
	m_pPreviewInput->SetMultiline( false );

	m_pContentLabel = new Label( this, NULL, "Addon Content:" );
	m_pContentInput = new TextEntry( this, "content" );
	m_pContentInput->SetMaximumCharCount( MAX_PATH - 1 );
	m_pContentInput->SetMultiline( false );

	m_pVisibilityLabel = new Label( this, NULL, "Visibility:" );
	m_pVisibility = new ComboBox( this, "visibility", 4, false );
	// NOTE: Add in the order of the raw values. This is read as int in UpdateFields
	m_pVisibility->AddItem( "Public", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityPublic ) );
	m_pVisibility->AddItem( "Friends Only", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityFriendsOnly ) );
	m_pVisibility->AddItem( "Private", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityPrivate ) );
	m_pVisibility->AddItem( "Unlisted", new KeyValues( "", NULL, k_ERemoteStoragePublishedFileVisibilityUnlisted_149 ) );
	m_pVisibility->SilentActivateItem( k_ERemoteStoragePublishedFileVisibilityPrivate );

	m_pPreview = new CPreviewImage( this, "PreviewImage" );

	m_pPreviewBrowse = new Button( this, "PreviewBrowse", "Browse...", this, "PreviewBrowse" );
	m_pContentBrowse = new Button( this, "ContentBrowse", "Browse...", this, "ContentBrowse" );

	m_pClose = new Button( this, "Cancel", "Cancel", this, "Close" );
	m_pPublish = new Button( this, "Publish", "Publish", this, "Publish" );

	m_pPreviewBrowse->SetContentAlignment( Label::Alignment::a_center );
	m_pContentBrowse->SetContentAlignment( Label::Alignment::a_center );
	m_pClose->SetContentAlignment( Label::Alignment::a_center );
	m_pPublish->SetContentAlignment( Label::Alignment::a_center );

	m_pTagsLabel = new Label( this, NULL, "Tags:" );

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

	if ( m_bDownloadedTempFiles )
	{
		char fullpath[MAX_PATH];
		GetPreviewImageTempLocation( m_item, fullpath, sizeof(fullpath) );

		if ( g_pFullFileSystem->FileExists( fullpath ) )
		{
			g_pFullFileSystem->RemoveFile( fullpath );
			DevMsg( "Remove temp file: '%s'\n", fullpath );
		}
	}

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
		m_pFileBrowser = new FileOpenDialog( this, "Browse preview image", FileOpenDialogType_t::FOD_OPEN );

		// Enable the filter that was chosen before
		bool bJPG = 0, bPNG = 0, bGIF = 0;
		switch ( g_pWorkshopPublish->m_nLastUsedFileFilter )
		{
			case FILE_FILTER_JPG:
				bJPG = true;
				break;
			case FILE_FILTER_PNG:
				bPNG = true;
				break;
			case FILE_FILTER_GIF:
				bGIF = true;
				break;
		}

		m_pFileBrowser->AddFilter( "*.*", "All files (*.*)", false );
		m_pFileBrowser->AddFilter( "*.jpg;*.jpeg", "JPEG (*.jpg;*.jpeg)", bJPG );
		m_pFileBrowser->AddFilter( "*.png", "PNG (*.png)", bPNG );
		m_pFileBrowser->AddFilter( "*.gif", "GIF (*.gif)", bGIF );

		m_pFileBrowser->AddActionSignalTarget( this );
		m_pFileBrowser->SetDeleteSelfOnClose( true );
		m_pFileBrowser->SetVisible( true );
		m_pFileBrowser->DoModal();
	}
	else if ( !V_stricmp( "ContentBrowse", command ) )
	{
		// NOTE: DirectorySelectDialog sucks, but FileOpenDialogType_t::FOD_SELECT_DIRECTORY does not work
		m_pDirectoryBrowser = new DirectorySelectDialog( this, "Browse content directory" );
		m_pDirectoryBrowser->MakeReadyForUse();
		m_pDirectoryBrowser->AddActionSignalTarget( this );
		m_pDirectoryBrowser->SetDeleteSelfOnClose( true );
		m_pDirectoryBrowser->SetVisible( true );
		m_pDirectoryBrowser->DoModal();

		char pLocalPath[255];
		g_pFullFileSystem->GetCurrentDirectory( pLocalPath, 255 );
		m_pDirectoryBrowser->SetStartDirectory( pLocalPath );
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

// Check if string consists only of spaces.
bool IsInvalidInput( const char *pIn )
{
	for ( const char *c = pIn; *c; ++c )
	{
		if ( !V_isspace(*c) )
		{
			return false;
		}
	}
	return true;
}

void CItemPublishDialog::OnPublish()
{
	char pszPreview[ 64 ];
	m_pPreviewInput->GetText( pszPreview, sizeof(pszPreview) );

	// Require preview image for initial commit
	if ( !m_item && IsInvalidInput( pszPreview ) )
	{
		Warning( "Preview image field empty\n" );
		m_pPreviewInput->SetText( "" );
		m_pPreviewInput->RequestFocus();
		return;
	}

	char pszContent[ 64 ];
	m_pContentInput->GetText( pszContent, sizeof(pszContent) );

	// Require content for initial commit
	if ( !m_item && IsInvalidInput( pszContent ) )
	{
		Warning( "Content field empty\n" );
		m_pContentInput->SetText( "" );
		m_pContentInput->RequestFocus();
		return;
	}

	char pszTitle[ k_cchPublishedDocumentTitleMax - 1 ];
	m_pTitleInput->GetText( pszTitle, sizeof(pszTitle) );

	// Require title
	if ( IsInvalidInput( pszTitle ) )
	{
		Warning( "Title field empty\n" );
		m_pTitleInput->SetText( "" );
		m_pTitleInput->RequestFocus();
		return;
	}

	char pszDesc[ 2 ];
	m_pDescInput->GetText( pszDesc, sizeof(pszDesc) );

	// Require description for initial commit
	if ( !m_item && IsInvalidInput( pszDesc ) )
	{
		Warning( "Description field empty\n" );
		m_pDescInput->SetText( "" );
		m_pDescInput->RequestFocus();
		return;
	}

	char pszChanges[ 2 ];
	m_pChangesInput->GetText( pszChanges, sizeof(pszChanges) );

	// If content is updated, request change notes once. (Allow initial commit without message)
	// This is also required in L4D2 and HLVR workshop tools.
	if ( m_item && pszContent[0] && IsInvalidInput( pszChanges ) )
	{
		Warning( "Change notes field empty\n" );
		m_pChangesInput->SetText( "Undocumented changes" );
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

void CItemPublishDialog::OnFileSelected( const char *fullpath )
{
	m_pPreviewInput->SetText( fullpath );

	const char *ext = V_GetFileExtension( fullpath );

	if ( !V_stricmp( ext, "jpg" ) || !V_stricmp( ext, "jpeg" ) )
	{
		m_pPreview->SetJPEGImage( fullpath );
		g_pWorkshopPublish->m_nLastUsedFileFilter = FILE_FILTER_JPG;
	}
	else if ( !V_stricmp( ext, "png" ) )
	{
		m_pPreview->SetPNGImage( fullpath );
		g_pWorkshopPublish->m_nLastUsedFileFilter = FILE_FILTER_PNG;
	}
	else if ( !V_stricmp( ext, "gif" ) )
	{
		g_pWorkshopPublish->m_nLastUsedFileFilter = FILE_FILTER_GIF;
	}

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
	SetTitle( CFmtStr("Item Publish (%llu)", m_item), false );

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

	m_pPublish->SetText( "Update" );
}


CModalMessageBox::CModalMessageBox( Panel *pParent, const char *msg ) :
	BaseClass( pParent, NULL, false, false )
{
	SetVisible( true );
	SetTitle( "", false );
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
	SetTitle( "Publishing...", false );
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

	m_pLabel = new Label( this, NULL, "Creating item..." );
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
			m_pLabel->SetText( "Processing configuration data..." );
			break;
		case k_EItemUpdateStatusPreparingContent:
			m_pLabel->SetText( "Reading and processing content files..." );
			break;
		case k_EItemUpdateStatusUploadingContent:
			m_pLabel->SetText( "Uploading content changes to Steam..." );
			break;
		case k_EItemUpdateStatusUploadingPreviewFile:
			m_pLabel->SetText( "Uploading new preview file image..." );
			break;
		case k_EItemUpdateStatusCommittingChanges:
			m_pLabel->SetText( "Committing all changes..." );
			break;
		case k_EItemUpdateStatusInvalid:
			Close();
			break;
		}
	}
}


CDeleteMessageBox::CDeleteMessageBox( Panel *pParent, const char *title, const char *msg ) : BaseClass( title, msg, pParent )
{
	SetCloseButtonVisible( true );
	SetDeleteSelfOnClose( true );
	SetSizeable( false );
	SetMoveable( false );

	MakePopup();
	DoModal();
	MoveToFront();

	// "#MessageBox_Cancel" wasn't translated, set labels manually
	m_pCancelButton->SetText( "Cancel" );
	m_pOkButton->SetText( "OK" );

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
	m_pOkButton->SetDepressedColor( m_pOkButton->GetButtonDepressedFgColor(), dark );
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
