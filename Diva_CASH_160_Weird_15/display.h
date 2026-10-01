#pragma once

#include "CompositeGraphics.h"
#include "Image.h"
#include "CompositeColorOutput.h"

#include "i_HeadlessLogo.h"

#include "i_CollectProduct.h"
#include "i_Dispensing.h"
#include "i_NoteAccepted.h"
#include "i_NoteProcessing.h"
#include "i_PleaseRefill.h"
#include "i_PushButtonForDispense.h"
#include "i_PushButtonForCashDispense.h"
#include "i_SyncComplete.h"
#include "i_Welcome.h"
#include "i_InsertTenTaka.h"
#include "i_TimeOut.h"

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);

Image<CompositeGraphics> logo_image(HeadlessLogo::xres, HeadlessLogo::yres, HeadlessLogo::pixels);

Image<CompositeGraphics> collectProduct_t(CollectProduct::xres, CollectProduct::yres, CollectProduct::pixels);
Image<CompositeGraphics> dispensing_t(Dispensing::xres, Dispensing::yres, Dispensing::pixels);
Image<CompositeGraphics> noteAccepted_t(NoteAccepted::xres, NoteAccepted::yres, NoteAccepted::pixels);
Image<CompositeGraphics> noteProcessing_t(NoteProcessing::xres, NoteProcessing::yres, NoteProcessing::pixels);
Image<CompositeGraphics> pleaseRefill_t(PleaseRefill::xres, PleaseRefill::yres, PleaseRefill::pixels);
Image<CompositeGraphics> pushButtonForDispense_t(PushButtonForDispense::xres, PushButtonForDispense::yres, PushButtonForDispense::pixels);
Image<CompositeGraphics> pushButtonForCashDispense_t(PushButtonForCashDispense::xres, PushButtonForCashDispense::yres, PushButtonForCashDispense::pixels);
Image<CompositeGraphics> syncComplete_t(SyncComplete::xres, SyncComplete::yres, SyncComplete::pixels);
Image<CompositeGraphics> welcome_t(Welcome::xres, Welcome::yres, Welcome::pixels);
Image<CompositeGraphics> insertTenTaka_t(InsertTenTaka::xres, InsertTenTaka::yres, InsertTenTaka::pixels);
Image<CompositeGraphics> timeOut_t(TimeOut::xres, TimeOut::yres, TimeOut::pixels);

void showImage(Image<CompositeGraphics> text) {
  graphics.begin(0);  //clear screen black
  logo_image.draw(graphics, 60, 40);
  text.draw(graphics, 90, 100);
  //  icon.draw(graphics, 251, 104);
  graphics.end();
  composite.sendFrameHalfResolution(&graphics.frame);
}