/* Author: Eric Wing */
/* Handle clipboard text and data in arbitrary formats */

#include <stdio.h>
#include <limits.h>

//#include <stdlib.h>
//#include <string.h>
#include "SDL.h"
#include "SDL_syswm.h"

#define my_malloc SDL_malloc
#define my_free SDL_free
#define my_strlen SDL_strlen
#define my_strcmp SDL_strcmp
#define my_memcpy SDL_memcpy
typedef const char SDLScrap_DataType;

/* Miscellaneous defines */
#define PUBLIC
#define PRIVATE	static

SDLScrap_DataType* SDL_CLIPBOARD_TEXT_TYPE = "SDL_CLIPBOARD_TEXT_TYPE (This string may be different on each platform.)";
SDLScrap_DataType* SDL_CLIPBOARD_IMAGE_TYPE = "SDL_CLIPBOARD_IMAGE_TYPE (This string may be different on each platform.)";

/* Determine what type of clipboard we are using */
#if defined(_WIN32)
    #define WIN_SCRAP
#elif defined(X11_SCRAP) || defined(__unix__) \
		|| defined (_BSD_SOURCE) || defined (_SYSV_SOURCE) \
        || defined (__FreeBSD__) || defined (__MACH__) \
        || defined (__OpenBSD__) || defined (__NetBSD__) \
        || defined (__bsdi__) || defined (__svr4__) \
        || defined (bsdi) || defined (__SVR4) \
		&& !defined(__QNXNTO__) \
		&& !defined(OSX_SCRAP) \
		&& !defined(DONT_USE_X11_SCRAP)
    #define X11_SCRAP
#elif defined(__QNXNTO__)
    #define QNX_SCRAP
#elif defined(__APPLE__) || defined(OSX_SCRAP)
	#error Do not compile this file for Mac OS X. Use scrap_OSX.m instead.
#else
/*
    #error Unknown window manager for clipboard handling
*/
    #warning Unknown window manager for clipboard handling, Will be disabled
#endif /* scrap type */

/* System dependent data types */
#if defined(X11_SCRAP)
/* * */
typedef Atom scrap_type;

#elif defined(WIN_SCRAP)
/* * */
typedef UINT scrap_type;

#elif defined(QNX_SCRAP)
/* * */
typedef uint32_t scrap_type;
#define Ph_CL_TEXT SDLSCRAP_TEXT

#else /* Unknown system type */
typedef int scrap_type;
#endif /* scrap type */

/* System dependent variables */
#if defined(X11_SCRAP)
/* * */
static Display *SDL_Display;
static Window SDL_Window;
static void (*Lock_Display)(void);
static void (*Unlock_Display)(void);

#elif defined(WIN_SCRAP)
/* * */
static HWND SDL_Window;

#elif defined(QNX_SCRAP)
/* * */
static unsigned short InputGroup;

#endif /* scrap type */

#define FORMAT_PREFIX	"SDL_scrap_0x"


PRIVATE scrap_type
convert_format(SDLScrap_DataType* type)
{
	if(!my_strcmp(SDL_CLIPBOARD_TEXT_TYPE, type))
	{
#if defined(X11_SCRAP)
/* * */
		return XA_STRING;

#elif defined(WIN_SCRAP)
/* * */
		return CF_TEXT;

#elif defined(QNX_SCRAP)
/* * */
		return Ph_CL_TEXT;

#endif /* scrap type */
	}
	else if(!my_strcmp(SDL_CLIPBOARD_IMAGE_TYPE, type))
	{
#if defined(WIN_SCRAP)
		/* Yikes, there is a format called CF_TIFF.
		 * TIFF would have been way better than BMP,
		 * and would have made things trivial on OS X.
		 */
		return CF_DIB;
#endif
	}
	else
	{
		/* FIXME: This probably doesn't work now that I've changed how types
		 * are expressed.
		 */
		char format[sizeof(FORMAT_PREFIX)+8+1];

		sprintf(format, "%s%08lx", FORMAT_PREFIX, (unsigned long)type);

#if defined(X11_SCRAP)
		/* * */
		return XInternAtom(SDL_Display, format, False);

#elif defined(WIN_SCRAP)
		/* * */
		/* FIXME: Is there a way we can make string identifiers generically
		 * work for all Windows cases?
		 */
        return RegisterClipboardFormat(type);

#endif /* scrap type */
	}

	return XA_STRING;
}

/* Convert internal data to scrap format */
PRIVATE int
convert_data(SDLScrap_DataType* type, char *dst, const char *src, int srclen)
{
  int dstlen;

  dstlen = 0;
	if(!my_strcmp(SDL_CLIPBOARD_TEXT_TYPE, type))
	{
      if ( dst )
        {
          while ( --srclen >= 0 )
            {
#if defined(__unix__)
              if ( *src == '\r' )
                {
                  *dst++ = '\n';
                  ++dstlen;
                }
              else
#elif defined(_WIN32)
              if ( *src == '\r' )
                {
                  *dst++ = '\r';
                  ++dstlen;
                  *dst++ = '\n';
                  ++dstlen;
                }
              else
#endif
                {
                  *dst++ = *src;
                  ++dstlen;
                }
              ++src;
            }
            *dst = '\0';
            ++dstlen;
        }
      else
        {
          while ( --srclen >= 0 )
            {
#if defined(__unix__)
              if ( *src == '\r' )
                {
                  ++dstlen;
                }
              else
#elif defined(_WIN32)
              if ( *src == '\r' )
                {
                  ++dstlen;
                  ++dstlen;
                }
              else
#endif
                {
                  ++dstlen;
                }
              ++src;
            }
            ++dstlen;
        }
	}

/* Ugh. For Windows, CF_DIB expects the BITMAPINFO+Data,
 * but not the BITMAPFILEHEADER. SDL gives us the whole
 * thing. So we need to remove the 14-byte header before
 * we pass the data to the clipboard. (Going the other
 * way is going to be worse because we have to construct
 * a header with the correct info.)
 */
#if defined(_WIN32)
	else if(!my_strcmp(SDL_CLIPBOARD_IMAGE_TYPE, type))
	{

		/* BITMAPFILEHEADER is 14-bytes according to all
		 * information I can find. So we just need to remove
		 * the first 14-bytes by not copying them in.
		 */
		if(srclen > sizeof(BITMAPFILEHEADER))
		{
			dstlen = srclen - sizeof(BITMAPFILEHEADER);

			if ( dst )
			{
				const char* adjusted_start_ptr;
				adjusted_start_ptr = &src[sizeof(BITMAPFILEHEADER)];
				my_memcpy(dst, adjusted_start_ptr, dstlen);
			}
		}
	}
#endif

	else
	{
      if ( dst )
        {
          *(int *)dst = srclen;
          dst += sizeof(int);
          my_memcpy(dst, src, srclen);
        }
      dstlen = sizeof(int)+srclen;
    }
    return(dstlen);
}

/* Convert scrap data to internal format */
PRIVATE int
convert_scrap(SDLScrap_DataType* type, char *dst, const char *src, int srclen)
{
  int dstlen;

  dstlen = 0;
	if(!my_strcmp(SDL_CLIPBOARD_TEXT_TYPE, type))
      {
        if ( srclen == 0 )
          srclen = my_strlen(src);
        if ( dst )
          {
            while ( --srclen >= 0 )
              {
#if defined(_WIN32)
                if ( *src == '\r' )
                  /* drop extraneous '\r' */;
                else
#endif
                //if ( *src == '\n' )
                //  {
                //    *dst++ = '\r';
               //     ++dstlen;
               //   }
				if( *src == '\r' )
				 {
					 printf("*is this reached?*");
					 *dst++ = '\n';
					 ++dstlen;
				 }
                else
                  {
                    *dst++ = *src;
                    ++dstlen;
                  }
                ++src;
              }
              *dst = '\0';
              ++dstlen;
          }
        else
          {
            while ( --srclen >= 0 )
              {
#if defined(_WIN32)
                if ( *src == '\r' )
                  /* drop extraneous '\r' */;
                else
#endif
                ++dstlen;
                ++src;
              }
              ++dstlen;
          }
        }
/* Ugh. Here's the hard part. Windows is returning us
 * a BITMAPINFO+Data, but no BITMAPFILEHEADER which the
 * SDL BMP functions require. So we need to construct
 * this header, populate it with the correct data,
 * and prepend it to the dst buffer. Filling it with
 * the correct data is going to be the ugly part since
 * I do not know the BMP file format.
 */

#if defined(_WIN32)
	else if(!my_strcmp(SDL_CLIPBOARD_IMAGE_TYPE, type))
	{
		/* BITMAPFILEHEADER is 14-bytes according to all
		 * information I can find. So we just need to remove
		 * the first 14-bytes by not copying them in.
		 */
			BITMAPFILEHEADER file_header;
			BITMAPINFOHEADER bm_header;
			/* This is {'B', 'M'} in hex */
			file_header.bfType = 0x4d42;
			file_header.bfReserved1 = 0;
			file_header.bfReserved2 = 0;

		/* To compute the bfOffBits, I need
		* to know the size of the BITMAPINFOHEADER.
		* I can't safely use sizeof(BITMAPINFOHEADER)
		* as my real size because the BITMAPV4HEADER
		* and BITMAPV5HEADER variants are larger. But,
		* I think the size is stored as the first
		* 32-bit value in the header which happens
		* to be the start of the src array.
		* I also need to know how much image data there is
		* in this stream. I hope to get that from the
		* biSizeImage field.
		*/
		my_memcpy(&bm_header, src, sizeof(BITMAPINFOHEADER));

		if(bm_header.biSizeImage == 0)
		{
			/* Can only be 0 if RGB. I'm assuming this
			* calculation is correct. I might be wrong due to padding.
			* 3 is for 3-bytes for RGB.
			*/
			int bytes_per_pixel = 3;
			int padding_bytes = 0;

			if(bm_header.biBitCount != 24)
			{
				SDL_SetError("Assertion failure: biBitCount is not 24: It is %d. The biCompression should be 0 for RGB, it is %d", bm_header.biBitCount, bm_header.biCompression);
				bytes_per_pixel = bm_header.biBitCount / 8;
			}

			/* If the width is not a multiple of 4, we will have
			 * alignment problems.
			 */
			if((bm_header.biWidth * bytes_per_pixel)%4)
			{
				padding_bytes = 4-(bm_header.biWidth%4);
			}
			bm_header.biSizeImage = (bm_header.biWidth+padding_bytes) * bm_header.biHeight * bytes_per_pixel;
		}

		/* This is typically 14 + 40 = 54 */
		file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + bm_header.biSize;
		/* This is typically 54 + data size */
		file_header.bfSize = file_header.bfOffBits + bm_header.biSizeImage;
		dstlen = file_header.bfSize;

		/* Debug stuff */
		/*
		printf("width=%d, height=%d, compression=%d\n", bm_header.biWidth, bm_header.biHeight, bm_header.biCompression);
		printf("plane=%d, bitcount=%d, sizeimage=%d\n", bm_header.biPlanes, bm_header.biBitCount, bm_header.biSizeImage);
		printf("BI_RGB=%d, BI_RLE8=%d, BI_RLE4=%d\n", BI_RGB, BI_RLE8, BI_RLE4);
		printf("bm_header.biSize=%d\n", bm_header.biSize);
		printf("file header bfSize=%d, bm_header.biSizeImage=%d, bfOffBits=%d\n", file_header.bfSize, bm_header.biSizeImage, file_header.bfOffBits);
		printf("dstlen=%d, srclen=%d\n", dstlen, dstlen-sizeof(BITMAPFILEHEADER));
		*/

		if ( dst )
		{
			char* adjusted_start_ptr;
			adjusted_start_ptr = dst;
			/* Copy file header into dst */
			my_memcpy(adjusted_start_ptr, &file_header, sizeof(BITMAPFILEHEADER));
			adjusted_start_ptr += sizeof(BITMAPFILEHEADER);

			/* Copy remaining data in clipboard */
			my_memcpy(adjusted_start_ptr, src, file_header.bfSize - sizeof(BITMAPFILEHEADER));
		}
	}
#endif /* WINSCRAP */
	else
	{
      dstlen = *(int *)src;
      if ( dst )
        {
          if ( srclen == 0 )
            my_memcpy(dst, src+sizeof(int), dstlen);
          else
            my_memcpy(dst, src+sizeof(int), srclen-sizeof(int));
        }
    }
  return dstlen;
}

#if defined(X11_SCRAP)
/* The system message filter function -- handle clipboard messages */
PRIVATE int clipboard_filter(const SDL_Event *event);
#endif

PUBLIC int
SDLScrap_Init()
{
  SDL_SysWMinfo info;
  int retval;

  /* Grab the window manager specific information */
  retval = -1;
  SDL_SetError("SDL is not running on known window manager");

  SDL_VERSION(&info.version);
  if ( SDL_GetWMInfo(&info) )
    {
      /* Save the information for later use */
#if defined(X11_SCRAP)
/* * */
      if ( info.subsystem == SDL_SYSWM_X11 )
        {
          SDL_Display = info.info.x11.display;
          SDL_Window = info.info.x11.window;
          Lock_Display = info.info.x11.lock_func;
          Unlock_Display = info.info.x11.unlock_func;

          /* Enable the special window hook events */
          SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
          SDL_SetEventFilter(clipboard_filter);

          retval = 0;
        }
      else
        {
          SDL_SetError("SDL is not running on X11");
        }

#elif defined(WIN_SCRAP)
/* * */
      SDL_Window = info.window;
      retval = 0;

#elif defined(QNX_SCRAP)
/* * */
      InputGroup=PhInputGroup(NULL);
      retval = 0;

#else
	  SDL_SetError("Clipboard (scrap) support not implemented for this platform");

#endif /* scrap type */
    }
  return(retval);
}

PUBLIC int
SDLScrap_Lost()
{
  int retval;

#if defined(X11_SCRAP)
/* * */
  Lock_Display();
  retval = ( XGetSelectionOwner(SDL_Display, XA_PRIMARY) != SDL_Window );
  Unlock_Display();

#elif defined(WIN_SCRAP)
/* * */
  retval = ( GetClipboardOwner() != SDL_Window );

#elif defined(QNX_SCRAP)
/* * */
  retval = ( PhInputGroup(NULL) != InputGroup );

#else
  retval = 1; /* Always say you 'lost scrap'? */

#endif /* scrap type */

  return(retval);
}

PUBLIC void
SDLScrap_CopyToClipboard(SDLScrap_DataType* type, size_t srclen, const char *src)
{
  scrap_type format;
  int dstlen;
  char *dst;

#if 0
  fprintf(stderr, "SDLScrap_CopyToClipboard type=%s\n", type);
#endif
  format = convert_format(type);
  dstlen = convert_data(type, NULL, src, srclen);

#if defined(X11_SCRAP)
/* * */
  dst = (char *)my_malloc(dstlen);
  if ( dst != NULL )
    {
      Lock_Display();
      convert_data(type, dst, src, srclen);
      XChangeProperty(SDL_Display, DefaultRootWindow(SDL_Display),
        XA_CUT_BUFFER0, format, 8, PropModeReplace, (unsigned char *) dst, dstlen);
      my_free(dst);
      if ( SDLScrap_Lost() )
        XSetSelectionOwner(SDL_Display, XA_PRIMARY, SDL_Window, CurrentTime);
      Unlock_Display();
    }

#elif defined(WIN_SCRAP)
/* * */
  if ( OpenClipboard(SDL_Window) )
    {
      HANDLE hMem;

      hMem = GlobalAlloc((GMEM_MOVEABLE|GMEM_DDESHARE), dstlen);
      if ( hMem != NULL )
        {
          dst = (char *)GlobalLock(hMem);
          convert_data(type, dst, src, srclen);
          GlobalUnlock(hMem);
          EmptyClipboard();
          SetClipboardData(format, hMem);
        }
      CloseClipboard();
    }

#elif defined(QNX_SCRAP)
/* * */
	#if 0  /* QNX */
  #if (_NTO_VERSION < 620) /* before 6.2.0 releases */
  {
	  /* FIXME: type no longer makes sense */
     PhClipHeader clheader={Ph_CLIPBOARD_TYPE_TEXT, 0, NULL};
     int* cldata;
     int status;

     dst = (char *)my_malloc(dstlen+4);
     if (dst != NULL)
     {
        cldata=(int*)dst;
        *cldata=type;
        convert_data(type, dst+4, src, srclen);
        clheader.data=dst;
        if (dstlen>65535)
        {
           clheader.length=65535; /* maximum photon clipboard size :( */
        }
        else
        {
           clheader.length=dstlen+4;
        }
        status=PhClipboardCopy(InputGroup, 1, &clheader);
        if (status==-1)
        {
           fprintf(stderr, "Photon: copy to clipboard was failed !\n");
        }
        my_free(dst);
     }
  }
  #else /* 6.2.0 and 6.2.1 and future releases */
  {
     PhClipboardHdr clheader={Ph_CLIPBOARD_TYPE_TEXT, 0, NULL};
     int* cldata;
     int status;

     dst = (char *)my_malloc(dstlen+4);
     if (dst != NULL)
     {
        cldata=(int*)dst;
        *cldata=type;
        convert_data(type, dst+4, src, srclen);
        clheader.data=dst;
        clheader.length=dstlen+4;
        status=PhClipboardWrite(InputGroup, 1, &clheader);
        if (status==-1)
        {
           fprintf(stderr, "Photon: copy to clipboard was failed !\n");
        }
        my_free(dst);
     }
  }
  #endif
  	#else /* QNX */
		#error QNX Implementation is currently broken
	#endif /* QNX */
#else /* Unknown system type */
  /* Do nothing */
#endif /* scrap type */
}

PUBLIC void
SDLScrap_PasteFromClipboard(SDLScrap_DataType* type, size_t *dstlen, char **dst)
{
  scrap_type format;

  *dstlen = 0;
#if 0
  fprintf(stderr, "SDLScrap_PasteFromClipboard type=%s\n", type);
#endif
  format = convert_format(type);

#if defined(X11_SCRAP)
/* * */
  {
    Window owner;
    Atom selection;
    Atom seln_type;
    int seln_format;
    unsigned long nbytes;
    unsigned long overflow;
    char *src;

    Lock_Display();
    owner = XGetSelectionOwner(SDL_Display, XA_PRIMARY);
    Unlock_Display();
    if ( (owner == None) || (owner == SDL_Window) )
      {
        owner = DefaultRootWindow(SDL_Display);
        selection = XA_CUT_BUFFER0;
      }
    else
      {
        int selection_response = 0;
        SDL_Event event;

        owner = SDL_Window;
        Lock_Display();
        selection = XInternAtom(SDL_Display, "SDL_SELECTION", False);
        XConvertSelection(SDL_Display, XA_PRIMARY, format,
                                        selection, owner, CurrentTime);
        Unlock_Display();
        while ( ! selection_response )
          {
            SDL_WaitEvent(&event);
            if ( event.type == SDL_SYSWMEVENT )
              {
                XEvent xevent = event.syswm.msg->event.xevent;

                if ( (xevent.type == SelectionNotify) &&
                     (xevent.xselection.requestor == owner) )
                    selection_response = 1;
              }
          }
      }
    Lock_Display();
    if ( XGetWindowProperty(SDL_Display, owner, selection, 0, INT_MAX/4,
                            False, format, &seln_type, &seln_format,
                       &nbytes, &overflow, (unsigned char **)&src) == Success )
      {
        if ( seln_type == format )
          {
            *dstlen = convert_scrap(type, NULL, src, nbytes);
            *dst = (char *)realloc(*dst, *dstlen);
            if ( *dst == NULL )
              *dstlen = 0;
            else
              convert_scrap(type, *dst, src, nbytes);
          }
        XFree(src);
      }
    }
    Unlock_Display();

#elif defined(WIN_SCRAP)
/* * */
  if ( IsClipboardFormatAvailable(format) && OpenClipboard(SDL_Window) )
    {
      HANDLE hMem;
      char *src;

      hMem = GetClipboardData(format);
      if ( hMem != NULL )
        {
          src = (char *)GlobalLock(hMem);
          *dstlen = convert_scrap(type, NULL, src, 0);
          *dst = (char *)realloc(*dst, *dstlen);
          if ( *dst == NULL )
            *dstlen = 0;
          else
            convert_scrap(type, *dst, src, 0);
          GlobalUnlock(hMem);
        }
      CloseClipboard();
    }
#elif defined(QNX_SCRAP)
/* * */
  #if (_NTO_VERSION < 620) /* before 6.2.0 releases */
  {
     void* clhandle;
     PhClipHeader* clheader;
     int* cldata;

     clhandle=PhClipboardPasteStart(InputGroup);
     if (clhandle!=NULL)
     {
        clheader=PhClipboardPasteType(clhandle, Ph_CLIPBOARD_TYPE_TEXT);
        if (clheader!=NULL)
        {
           cldata=clheader->data;
           if ((clheader->length>4) && (*cldata==type))
           {
              *dstlen = convert_scrap(type, NULL, (char*)clheader->data+4, clheader->length-4);
              *dst = (char *)realloc(*dst, *dstlen);
              if (*dst == NULL)
              {
                 *dstlen = 0;
              }
              else
              {
                 convert_scrap(type, *dst, (char*)clheader->data+4, clheader->length-4);
              }
           }
        }
        PhClipboardPasteFinish(clhandle);
     }
  }
  #else /* 6.2.0 and 6.2.1 and future releases */
  {
     void* clhandle;
     PhClipboardHdr* clheader;
     int* cldata;

     clheader=PhClipboardRead(InputGroup, Ph_CLIPBOARD_TYPE_TEXT);
     if (clheader!=NULL)
     {
        cldata=clheader->data;
        if ((clheader->length>4) && (*cldata==type))
        {
           *dstlen = convert_scrap(type, NULL, (char*)clheader->data+4, clheader->length-4);
           *dst = (char *)realloc(*dst, *dstlen);
           if (*dst == NULL)
           {
              *dstlen = 0;
           }
           else
           {
              convert_scrap(type, *dst, (char*)clheader->data+4, clheader->length-4);
           }
        }
     }
  }
  #endif
#else /* Unknown system type */
  /* Do nothing */
#endif /* scrap type */
}

#if defined(X11_SCRAP)
PRIVATE int clipboard_filter(const SDL_Event *event)
{
  /* Post all non-window manager specific events */
  if ( event->type != SDL_SYSWMEVENT ) {
    return(1);
  }

  /* Handle window-manager specific clipboard events */
  switch (event->syswm.msg->event.xevent.type) {
    /* Copy the selection from XA_CUT_BUFFER0 to the requested property */
    case SelectionRequest: {
      XSelectionRequestEvent *req;
      XEvent sevent;
      int seln_format;
      unsigned long nbytes;
      unsigned long overflow;
      unsigned char *seln_data;

      req = &event->syswm.msg->event.xevent.xselectionrequest;
      sevent.xselection.type = SelectionNotify;
      sevent.xselection.display = req->display;
      sevent.xselection.selection = req->selection;
      sevent.xselection.target = None;
      sevent.xselection.property = None;
      sevent.xselection.requestor = req->requestor;
      sevent.xselection.time = req->time;
      if ( XGetWindowProperty(SDL_Display, DefaultRootWindow(SDL_Display),
                              XA_CUT_BUFFER0, 0, INT_MAX/4, False, req->target,
                              &sevent.xselection.target, &seln_format,
                              &nbytes, &overflow, &seln_data) == Success )
        {
          if ( sevent.xselection.target == req->target )
            {
              if ( sevent.xselection.target == XA_STRING )
                {
                  if ( seln_data[nbytes-1] == '\0' )
                    --nbytes;
                }
              XChangeProperty(SDL_Display, req->requestor, req->property,
                sevent.xselection.target, seln_format, PropModeReplace,
                                                      seln_data, nbytes);
              sevent.xselection.property = req->property;
            }
          XFree(seln_data);
        }
      XSendEvent(SDL_Display,req->requestor,False,0,&sevent);
      XSync(SDL_Display, False);
    }
    break;
  }

  /* Post the event for X11 clipboard reading above */
  return(1);
}

#endif /* X11_SCRAP */


void SDLScrap_FreeBuffer(char* clipboard_buffer)
{
	free(clipboard_buffer);
}
