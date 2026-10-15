#pragma once

#include "display_CompositeGraphics.h"
#include "display_Image.h"
#include "display_CompositeColorOutput.h"

#include "i_HeadlessLogo.h"

#if defined(HAS_RFID_MODULE)
#include "i_OnlyRfid.h"
#endif

#include "i_CollectProduct.h"
#include "i_Dispensing.h"
#include "i_AllInOne.h"
#include "i_Refill.h"
#include "i_Welcome.h"
#include "i_SyncComplete.h"
#include "i_TimeOut.h"

#if defined(HAS_RFID_MODULE)
#include "i_RfidProcessing.h"
#endif

#if defined(HAS_CASH_MODULE)
#include "i_InsertTenTaka.h"
#include "i_NoteProcessing.h"
#include "i_NoteAccepted.h"
#endif

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);

Image<CompositeGraphics> logo_image(HeadlessLogo::xres, HeadlessLogo::yres, HeadlessLogo::pixels);

void showImage(Image<CompositeGraphics> text);

#if defined(HAS_RFID_MODULE)
Image<CompositeGraphics> onlyRfid_t(OnlyRfid::xres, OnlyRfid::yres, OnlyRfid::pixels);
#endif

Image<CompositeGraphics> collectProduct_t(CollectProduct::xres, CollectProduct::yres, CollectProduct::pixels);
Image<CompositeGraphics> dispensing_t(Dispensing::xres, Dispensing::yres, Dispensing::pixels);
Image<CompositeGraphics> allInOne_t(AllInOne::xres, AllInOne::yres, AllInOne::pixels);
Image<CompositeGraphics> refill_t(Refill::xres, Refill::yres, Refill::pixels);
Image<CompositeGraphics> welcome_t(Welcome::xres, Welcome::yres, Welcome::pixels);
Image<CompositeGraphics> syncComplete_t(SyncComplete::xres, SyncComplete::yres, SyncComplete::pixels);

#if defined(HAS_RFID_MODULE)
Image<CompositeGraphics> rfidProcessing_t(RfidProcessing::xres, RfidProcessing::yres, RfidProcessing::pixels);
#endif

#if defined(HAS_CASH_MODULE)
Image<CompositeGraphics> insertTenTaka_t(InsertTenTaka::xres, InsertTenTaka::yres, InsertTenTaka::pixels);
Image<CompositeGraphics> noteProcessing_t(NoteProcessing::xres, NoteProcessing::yres, NoteProcessing::pixels);
Image<CompositeGraphics> noteAccepted_t(NoteAccepted::xres, NoteAccepted::yres, NoteAccepted::pixels);
Image<CompositeGraphics> timeOut_t(TimeOut::xres, TimeOut::yres, TimeOut::pixels);
#endif


void showImage(Image<CompositeGraphics> img) {
  graphics.begin(0); // clear screen black
  logo_image.draw(graphics, 60, 40);
  img.draw(graphics, 90, 100);
  graphics.end();
  composite.sendFrameHalfResolution(&graphics.frame);
}


void showModuleImage() {
  #if defined(HAS_RFID_MODULE) && defined(HAS_CASH_MODULE)
    showImage(allInOne_t);
  #elif defined(HAS_RFID_MODULE)
    showImage(onlyRfid_t);
  #else
    showImage(allInOne_t);
  #endif
}
