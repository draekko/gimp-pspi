changequote([,])dnl

dnl PROXY(type,suite,field,proc,arglist,fmtlist,argnames,resultfmt,outfmtlist,outnames)
dnl       $1   $2    $3    $4   $5      $6      $7       $8        $9

define(PROXY,[dnl
static $1
ifelse([$2],,,[$2_])$4 [$5]
{
ifelse($1,[void],,[  $1 result;
])dnl
ifelse($1,[OSErr],[  gchar *e;
])dnl
  fprintf (log, "ifelse([$2],,,[$2::])$4 ($6)"ifelse([$6],,,[, [$7]]));
  ifelse([$1],[void],,[result = ])host_filter[]ifelse([$3],,,[->$3])->$4 ([$7]);
ifelse([$1],[void],,[$1],[OSErr],[  e = error_string (result);
  fprintf (log, ": %s", e);
],[ifelse([$8],,,[  fprintf (log, ": $8", result);
])])dnl
ifelse([$9],,,[  fprintf (log, ": $9\n", $10);
])dnl
  fprintf (log, "\n");
ifelse([$1],[void],,[  return result;
])dnl
}dnl
ifelse([$2],,,[define([SUITEINITS],SUITEINITS[p$2->$4 = $2_$4;])])])dnl

define(SUITEINITS,[])

define(FMTTAG,[%s])
define(STRTAG,[int32_as_be_4c])

PROXY(OSErr, BufferProcs, bufferProcs, allocateProc,
	[(int32     size,
          BufferID *bufferID)],
	[%ld, %p], [size, bufferID], [], [%p], [*bufferID])

PROXY(Ptr, BufferProcs, bufferProcs, lockProc,
	[(BufferID bufferID,
	  Boolean  moveHigh)],
	[%p, %d], [bufferID, moveHigh], [%p])

PROXY(void, BufferProcs, bufferProcs, unlockProc,
	[(BufferID bufferID)],
	[%p], [bufferID])

PROXY(void, BufferProcs, bufferProcs, freeProc,
	[(BufferID bufferID)],
	[%p], [bufferID])

PROXY(int32, BufferProcs, bufferProcs, spaceProc,
	[(void)],
	[], [], [%ld])

PROXY(OSErr, ChannelPortProcs, channelPortProcs, readPixelsProc,
	[(ChannelReadPort 	      port,
          const PSScaling 	     *scaling,
	  const VRect     	     *writeRect,
	  const PixelMemoryDesc *destination,
	  VRect                 *wroteRect)],
	[], [port, scaling, writeRect, destination, wroteRect])

PROXY(OSErr, ChannelPortProcs, channelPortProcs, writeBasePixelsProc,
	[(ChannelWritePort 	      port,
	  const VRect     	     *writeRect,
	  const PixelMemoryDesc      *source)],
	[], [port, writeRect, source])

PROXY(OSErr, ChannelPortProcs, channelPortProcs, readPortForWritePortProc,
	[(ChannelReadPort *readPort,
          ChannelWritePort writePort)],
	[], [readPort, writePort])

PROXY(Handle, HandleProcs, handleProcs, newProc,
	[(int32 size)],
	[%ld], [size], [%p])

PROXY(void, HandleProcs, handleProcs, disposeProc,
	[(Handle h)],
	[%p], [h])

PROXY(int32, HandleProcs, handleProcs, getSizeProc,
	[(Handle h)],
	[%p], [h], [%ld])

PROXY(OSErr, HandleProcs, handleProcs, setSizeProc,
	[(Handle h,
	  int32  newSize)],
	[%p, %ld], [h, newSize])

PROXY(Ptr, HandleProcs, handleProcs, lockProc,
	[(Handle  h,
	  Boolean moveHigh)],
	[%p, %d], [h, moveHigh], [%p])

PROXY(void, HandleProcs, handleProcs, unlockProc,
	[(Handle h)],
	[%p], [h])

PROXY(void, HandleProcs, handleProcs, recoverSpaceProc,
	[(int32 size)],
	[%ld], [size])

PROXY(void, HandleProcs, handleProcs, disposeRegularHandleProc,
	[(Handle h)],
	[%p], [h])

PROXY(OSErr, ImageServicesProcs, imageServicesProcs, interpolate1DProc,
	[(PSImagePlane *source,
          PSImagePlane *destination,
	  Rect         *area,
	  Fixed        *coords,
	  int16         method)],
	[], [source, destination, area, coords, method])

PROXY(OSErr, ImageServicesProcs, imageServicesProcs, interpolate2DProc,
	[(PSImagePlane *source,
          PSImagePlane *destination,
	  Rect         *area,
	  Fixed        *coords,
	  int16         method)],
	[], [source, destination, area, coords, method])

PROXY(OSErr, ImageServicesProcs, imageServicesProcs, interpolate1DMultiProc,
	[(PSImageMultiPlane *source,
          PSImageMultiPlane *destination,
	  Rect              *area,
	  Fixed             *coords,
	  int16              method)],
	[], [source, destination, area, coords, method])

PROXY(OSErr, ImageServicesProcs, imageServicesProcs, interpolate2DMultiProc,
	[(PSImageMultiPlane *source,
          PSImageMultiPlane *destination,
	  Rect         	    *area,
	  Fixed        	    *coords,
	  int16        	     method)],
	[], [source, destination, area, coords, method])

PROXY(OSErr, PropertyProcs, propertyProcs, getPropertyProc,
	[(PIType  signature,
	  PIType  key,
 	  int32   index,
	  int32  *simpleProperty,
	  Handle *complexProperty)],
	[], [signature, key, index, simpleProperty, complexProperty])

PROXY(OSErr, PropertyProcs, propertyProcs, setPropertyProc,
	[(PIType signature,
	  PIType key,
 	  int32  index,
	  int32  simpleProperty,
	  Handle complexProperty)],
	[], [signature, key, index, simpleProperty, complexProperty])

PROXY(int16, ResourceProcs, resourceProcs, countProc,
	[(ResType ofType)],
	[%08x], [ofType], [%d])

PROXY(Handle, ResourceProcs, resourceProcs, getProc,
	[(ResType ofType,
	  int16   index)],
	[%08x, %d], [ofType, index], [%p])

PROXY(void, ResourceProcs, resourceProcs, deleteProc,
	[(ResType ofType,
	  int16   index)],
	[%08x, %d], [ofType, index])

PROXY(OSErr, ResourceProcs, resourceProcs, addProc,
	[(ResType ofType,
	  Handle  data)],
	[%08x, %p], [ofType, data])

PROXY(SPErr, SPBasicSuite, sSPBasic, AcquireSuite,
	[(char  *name,
	  long   version,
	  void **suite)],
	[%s, %ld, %p], [name, version, suite], [%ld], [%p], [*suite])

PROXY(SPErr, SPBasicSuite, sSPBasic, ReleaseSuite,
	[(char *name,
	  long  version )],
	[%s, %d], [name, version], [%ld])

PROXY(SPBoolean, SPBasicSuite, sSPBasic, IsEqual,
	[(char *token1,
	  char *token2 )],
	[%s, %s], [token1, token2])

PROXY(SPErr, SPBasicSuite, sSPBasic, AllocateBlock,
	[(long   size,
	  void **block)],
	[%ld, %p], [size, block], [%ld], [%p], [*block])

PROXY(SPErr, SPBasicSuite, sSPBasic, FreeBlock,
	[(void *block)],
	[%p], [block], [%ld])

PROXY(SPErr, SPBasicSuite, sSPBasic, ReallocateBlock,
	[(void  *block,
	  long   newSize,
	  void **newblock)],
	[%p, %ld, %p], [block, newSize, newblock], [%ld], [%p], [*newblock])

PROXY(SPErr, SPBasicSuite, sSPBasic, Undefined,
	[(void)],
	[], [], [%ld])

PROXY(Boolean, , , abortProc,
	[(void)],
	[], [], [%d])

PROXY(void, , , progressProc,
	[(long done,
	  long total)],
	[%ld, %ld], [done, total])

PROXY(void, , , hostProc,
	[(int16  selector,
	  int32 *data)],
	[%d, %ld], [selector, data])

PROXY(OSErr, , , colorServices,
	[(ColorServicesInfo *info)],
	[%p], [info])

PROXY(void, , , processEvent,
	[(void *event)],
	[%p], [event])

PROXY(OSErr, , , displayPixels,
	[(const PSPixelMap *source,
	  const VRect      *srcRect,
	  int32             dstRow,
	  int32             dstCol,
	  void             *platformContext)],
	[], [source, srcRect, dstRow, dstCol, platformContext])

[#define SUITEINITS()] SUITEINITS
