#pragma once

#include "display_CompositeGraphics.h"
#include "display_Image.h"
#include "display_CompositeColorOutput.h"

#include "i_HeadlessLogo.h"

#include "i_BkashComplete.h"
#include "i_BkashFailed.h"
#include "i_CollectProduct.h"
#include "i_Connecting.h"
#include "i_Dispensing.h"
#include "i_AllInOne.h"
#include "i_OnlyBkash.h"
#include "i_OnlyRfid.h"
#include "i_RfidAndBkash.h"
#include "i_Refill.h"
#include "i_Welcome.h"

#include "i_RfidProcessing.h"
#include "i_RfidBlocked.h"
#include "i_RfidLimitCrossed.h"
#include "i_UnauthorizedRequest.h"

#include "i_InsertTenTaka.h"
#include "i_NoteProcessing.h"
#include "i_NoteAccepted.h"

#include "i_SomethingWentWrong.h"
#include "i_TryAgain.h"

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);

Image<CompositeGraphics> logo_image(HeadlessLogo::xres, HeadlessLogo::yres, HeadlessLogo::pixels);

void showImage(Image<CompositeGraphics> text);

Image<CompositeGraphics> bkashComplete_t(BkashComplete::xres, BkashComplete::yres, BkashComplete::pixels);
Image<CompositeGraphics> bkashFailed_t(BkashFailed::xres, BkashFailed::yres, BkashFailed::pixels);
Image<CompositeGraphics> collectProduct_t(CollectProduct::xres, CollectProduct::yres, CollectProduct::pixels);
Image<CompositeGraphics> connecting_t(Connecting::xres, Connecting::yres, Connecting::pixels);
Image<CompositeGraphics> dispensing_t(Dispensing::xres, Dispensing::yres, Dispensing::pixels);
Image<CompositeGraphics> allInOne_t(AllInOne::xres, AllInOne::yres, AllInOne::pixels);
Image<CompositeGraphics> onlyBkash_t(OnlyBkash::xres, OnlyBkash::yres, OnlyBkash::pixels);
Image<CompositeGraphics> onlyRfid_t(OnlyRfid::xres, OnlyRfid::yres, OnlyRfid::pixels);
Image<CompositeGraphics> rfidAndBkash_t(RfidAndBkash::xres, RfidAndBkash::yres, RfidAndBkash::pixels);
Image<CompositeGraphics> refill_t(Refill::xres, Refill::yres, Refill::pixels);
Image<CompositeGraphics> welcome_t(Welcome::xres, Welcome::yres, Welcome::pixels);

Image<CompositeGraphics> rfidProcessing_t(RfidProcessing::xres, RfidProcessing::yres, RfidProcessing::pixels);
Image<CompositeGraphics> rfidBlocked_t(RfidBlocked::xres, RfidBlocked::yres, RfidBlocked::pixels);
Image<CompositeGraphics> rfidLimitCrossed_t(RfidLimitCrossed::xres, RfidLimitCrossed::yres, RfidLimitCrossed::pixels);
Image<CompositeGraphics> unauthorizedRequest_t(UnauthorizedRequest::xres, UnauthorizedRequest::yres, UnauthorizedRequest::pixels);

Image<CompositeGraphics> insertTenTaka_t(InsertTenTaka::xres, InsertTenTaka::yres, InsertTenTaka::pixels);
Image<CompositeGraphics> noteProcessing_t(NoteProcessing::xres, NoteProcessing::yres, NoteProcessing::pixels);
Image<CompositeGraphics> noteAccepted_t(NoteAccepted::xres, NoteAccepted::yres, NoteAccepted::pixels);

Image<CompositeGraphics> somethingWentWrong_t(SomethingWentWrong::xres, SomethingWentWrong::yres, SomethingWentWrong::pixels);
Image<CompositeGraphics> tryAgain_t(TryAgain::xres, TryAgain::yres, TryAgain::pixels);


void showImage(Image<CompositeGraphics> img) {
  graphics.begin(0); // clear screen black
  logo_image.draw(graphics, 60, 40);
  img.draw(graphics, 90, 100);
  graphics.end();
  composite.sendFrameHalfResolution(&graphics.frame);
}


void showModuleImage() {
	#if defined(HAS_RFID_MODULE) && defined(HAS_CASH_MODULE) && defined(HAS_BKASH_MODULE)
	    showImage(allInOne_t);
	    // showImage(all_modules);
	#elif defined(HAS_RFID_MODULE) && defined(HAS_CASH_MODULE)
      showImage(allInOne_t);
	    // showImage(rfid_cash);
	#elif defined(HAS_RFID_MODULE) && defined(HAS_BKASH_MODULE)
      showImage(rfidAndBkash_t);
	    // showImage(rfid_bkash);
	#elif defined(HAS_CASH_MODULE) && defined(HAS_BKASH_MODULE)
      showImage(allInOne_t);
	    // showImage(cash_bkash);
	#elif defined(HAS_RFID_MODULE)
      showImage(onlyRfid_t);
	#elif defined(HAS_CASH_MODULE)
      showImage(allInOne_t);
	    // showImage(only_Cash);
	#elif defined(HAS_BKASH_MODULE)
	    showImage(onlyBkash_t);
	#else
      showImage(allInOne_t);
	    // Optionally handle the case where no module is defined
	#endif
}
