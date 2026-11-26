#pragma once

#include "display_CompositeGraphics.h"
#include "display_Image.h"
#include "display_CompositeColorOutput.h"

#include "i_HeadlessLogo.h"

#if defined(HAS_BKASH_MODULE)
#include "i_BkashComplete.h"
#include "i_BkashFailed.h"
#include "i_OnlyBkash.h"
#endif

#if defined(HAS_RFID_MODULE)
#include "i_OnlyRfid.h"
#endif

#if defined(HAS_RFID_MODULE) && defined(HAS_BKASH_MODULE)
#include "i_RfidAndBkash.h"
#endif

#include "i_CollectProduct.h"
#include "i_Connecting.h"
#include "i_Dispensing.h"
#include "i_AllInOne.h"
#include "i_Refill.h"
#include "i_Welcome.h"

#if defined(HAS_RFID_MODULE)
#include "i_RfidBlocked.h"
#include "i_RfidLimitCrossed.h"
#include "i_RfidProcessing.h"
#endif
#include "i_UnauthorizedRequest.h"

#if defined(HAS_CASH_MODULE)
#include "i_InsertTenTaka.h"
#include "i_NoteProcessing.h"
#include "i_NoteAccepted.h"
#endif

#include "i_SomethingWentWrong.h"
#include "i_TryAgain.h"

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);

// Screen layout on the 336x240 frame. Only these two areas are ever drawn;
// the rest of the screen stays black and has no memory of its own (see
// CompositeGraphics::init).
constexpr int LOGO_X = 60, LOGO_Y = 40;     // Headless logo, drawn once at boot
constexpr int IMAGE_X = 90, IMAGE_Y = 100;  // the current message image
constexpr unsigned int IMAGE_WIDTH = 168, IMAGE_HEIGHT = 124;

// Declares a message image and checks its size at build time: every image
// is drawn over the previous one, so all must be exactly IMAGE_WIDTH x
// IMAGE_HEIGHT.
#define DIVA_IMAGE(var, ns)                                                   \
  static_assert(ns::xres == IMAGE_WIDTH && ns::yres == IMAGE_HEIGHT,          \
                #ns " must be 168x124 pixels like every other screen image"); \
  Image<CompositeGraphics> var(ns::xres, ns::yres, ns::pixels)

Image<CompositeGraphics> logo_image(HeadlessLogo::xres, HeadlessLogo::yres, HeadlessLogo::pixels);

void showImage(Image<CompositeGraphics>& img);

#if defined(HAS_BKASH_MODULE)
DIVA_IMAGE(bkashComplete_t, BkashComplete);
DIVA_IMAGE(bkashFailed_t, BkashFailed);
DIVA_IMAGE(onlyBkash_t, OnlyBkash);
#endif

#if defined(HAS_RFID_MODULE)
DIVA_IMAGE(onlyRfid_t, OnlyRfid);
#endif

#if defined(HAS_RFID_MODULE) && defined(HAS_BKASH_MODULE)
DIVA_IMAGE(rfidAndBkash_t, RfidAndBkash);
#endif

DIVA_IMAGE(collectProduct_t, CollectProduct);
DIVA_IMAGE(connecting_t, Connecting);
DIVA_IMAGE(dispensing_t, Dispensing);
DIVA_IMAGE(allInOne_t, AllInOne);
DIVA_IMAGE(refill_t, Refill);
DIVA_IMAGE(welcome_t, Welcome);

#if defined(HAS_RFID_MODULE)
DIVA_IMAGE(rfidProcessing_t, RfidProcessing);
DIVA_IMAGE(rfidBlocked_t, RfidBlocked);
DIVA_IMAGE(rfidLimitCrossed_t, RfidLimitCrossed);
#endif
DIVA_IMAGE(unauthorizedRequest_t, UnauthorizedRequest);

#if defined(HAS_CASH_MODULE)
DIVA_IMAGE(insertTenTaka_t, InsertTenTaka);
DIVA_IMAGE(noteProcessing_t, NoteProcessing);
DIVA_IMAGE(noteAccepted_t, NoteAccepted);
#endif

DIVA_IMAGE(somethingWentWrong_t, SomethingWentWrong);
DIVA_IMAGE(tryAgain_t, TryAgain);


bool screenRowIsDrawn(int y) {
  return (y >= LOGO_Y && y < LOGO_Y + (int)HeadlessLogo::yres) ||
         (y >= IMAGE_Y && y < IMAGE_Y + (int)IMAGE_HEIGHT);
}

// Call once in setup(), before the first showImage().
void displayInit() {
  composite.init();  // starts the video signal; shows nothing until frame is set
  graphics.init(screenRowIsDrawn);
  logo_image.draw(graphics, LOGO_X, LOGO_Y);  // never changes
  composite.sendFrameHalfResolution(&graphics.frame);
}

static const unsigned char* shownImagePixels = nullptr;

// Shows img in the message area. It is drawn only when the picture changes
// (about 1 ms), so calling this on every loop pass costs nothing.
void showImage(Image<CompositeGraphics>& img) {
  if (img.pixels == shownImagePixels) return;
  img.draw(graphics, IMAGE_X, IMAGE_Y);
  shownImagePixels = img.pixels;
}


void showModuleImage() {
  #if defined(HAS_RFID_MODULE) && defined(HAS_CASH_MODULE) && defined(HAS_BKASH_MODULE)
    showImage(allInOne_t);
  #elif defined(HAS_RFID_MODULE) && defined(HAS_CASH_MODULE)
    showImage(allInOne_t);
  #elif defined(HAS_RFID_MODULE) && defined(HAS_BKASH_MODULE)
    showImage(rfidAndBkash_t);
  #elif defined(HAS_CASH_MODULE) && defined(HAS_BKASH_MODULE)
    showImage(allInOne_t);
  #elif defined(HAS_RFID_MODULE)
    showImage(onlyRfid_t);
  #elif defined(HAS_CASH_MODULE)
    showImage(allInOne_t);
  #elif defined(HAS_BKASH_MODULE)
    showImage(onlyBkash_t);
  #else
    showImage(allInOne_t);
  #endif
}
