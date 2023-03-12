#ifndef _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA
#define _ESP32_VIRTUAL_MATRIX_PANEL_I2S_DMA

/*******************************************************************
    Class contributed by Brian Lough, and expanded by Faptastic.

    Originally designed to allow CHAINING of panels together to create
    a 'bigger' display of panels. i.e. Chaining 4 panels into a 2x2
    grid.

    However, the function of this class has expanded now to also manage
    the output for
	
	1) TWO scan panels = Two rows updated in parallel. 
		* 64px high panel =  sometimes referred to as 1/32 scan
		* 32px high panel =  sometimes referred to as 1/16 scan
		* 16px high panel =  sometimes referred to as 1/8 scan
		
	2) FOUR scan panels = Four rows updated in parallel
		* 32px high panel = sometimes referred to as 1/8 scan 
		* 16px high panel = sometimes referred to as 1/4 scan 
		
    YouTube: https://www.youtube.com/brianlough
    Tindie: https://www.tindie.com/stores/brianlough/
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#ifndef NO_GFX
#include <Fonts/FreeSansBold12pt7b.h>
#endif

//#include <iostream>

struct VirtualCoords
{
  int16_t x;
  int16_t y;
  int16_t virt_row; // chain of panels row
  int16_t virt_col; // chain of panels col

  VirtualCoords() : x(0), y(0)
  {
  }
};

enum PANEL_SCAN_RATE
{
  NORMAL_TWO_SCAN, NORMAL_ONE_SIXTEEN, // treated as the same
  FOUR_SCAN_32PX_HIGH,
  FOUR_SCAN_16PX_HIGH
};

// Chaining approach... From the perspective of the DISPLAY / LED side of the chain of panels.
enum PANEL_CHAIN_TYPE
{
	CHAIN_TOP_LEFT_DOWN,
	CHAIN_TOP_RIGHT_DOWN,
	CHAIN_BOTTOM_LEFT_UP,
	CHAIN_BOTTOM_RIGHT_UP
};

#ifdef USE_GFX_ROOT
class VirtualMatrixPanel : public GFX
#elif !defined NO_GFX
class VirtualMatrixPanel : public Adafruit_GFX
#else
class VirtualMatrixPanel
#endif
{

public:

  VirtualMatrixPanel(MatrixPanel_I2S_DMA &disp, int _vmodule_rows, int _vmodule_cols, int _panelResX, int _panelResY, PANEL_CHAIN_TYPE _panel_chain_type = CHAIN_TOP_RIGHT_DOWN)
#ifdef USE_GFX_ROOT
      : GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#elif !defined NO_GFX
      : Adafruit_GFX(_vmodule_cols * _panelResX, _vmodule_rows * _panelResY)
#endif
  {
    this->display = &disp;

    panel_chain_type = _panel_chain_type;

    panelResX = _panelResX;
    panelResY = _panelResY;

    vmodule_rows = _vmodule_rows;
    vmodule_cols = _vmodule_cols;

    virtualResX = vmodule_cols * _panelResX;
    virtualResY = vmodule_rows * _panelResY;

    dmaResX = panelResX * vmodule_rows * vmodule_cols - 1;

    /* Virtual Display width() and height() will return a real-world value. For example:
     * Virtual Display width: 128
     * Virtual Display height: 64
     *
     * So, not values that at 0 to X-1
     */

    coords.x = coords.y = -1; // By default use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer
  }

  // equivalent methods of the matrix library so it can be just swapped out.
  virtual void drawPixel(int16_t x, int16_t y, uint16_t color);
  virtual void fillScreen(uint16_t color); // overwrite adafruit implementation
  virtual void fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b);

  void clearScreen() { display->clearScreen(); }
  void drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);

#ifdef USE_GFX_ROOT
  // 24bpp FASTLED CRGB colour struct support
  void fillScreen(CRGB color);
  void drawPixel(int16_t x, int16_t y, CRGB color);
#endif

  uint16_t color444(uint8_t r, uint8_t g, uint8_t b) { return display->color444(r, g, b); }
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return display->color565(r, g, b); }
  uint16_t color333(uint8_t r, uint8_t g, uint8_t b) { return display->color333(r, g, b); }

  void flipDMABuffer() { display->flipDMABuffer(); }
  void drawDisplayTest();
  void setRotate(bool rotate);

  void setPhysicalPanelScanRate(PANEL_SCAN_RATE rate);

private:
  MatrixPanel_I2S_DMA *display;

  PANEL_CHAIN_TYPE 	panel_chain_type;    
  PANEL_SCAN_RATE 	panel_scan_rate    = NORMAL_TWO_SCAN;     

  virtual VirtualCoords getCoords(int16_t &x, int16_t &y);
  VirtualCoords coords;

  int16_t virtualResX;
  int16_t virtualResY;

  int16_t vmodule_rows;
  int16_t vmodule_cols;

  int16_t panelResX;
  int16_t panelResY;

  int16_t dmaResX; // The width of the chain in pixels (as the DMA engine sees it)

  bool _rotate = false;

}; // end Class header

/**
 * Calculate virtual->real co-ordinate mapping to underlying single chain of panels connected to ESP32.
 * Updates the private class member variable 'coords', so no need to use the return value.
 * Not thread safe, but not a concern for ESP32 sketch anyway... I think.
 */
inline VirtualCoords VirtualMatrixPanel::getCoords(int16_t &virt_x, int16_t &virt_y)
{
    coords.x = coords.y = -1; // By defalt use an invalid co-ordinates that will be rejected by updateMatrixDMABuffer

	if (virt_x < 0 || virt_x >= virtualResX || virt_y < 0 || virt_y >= virtualResY)
	{ // Co-ordinates go from 0 to X-1 remember! otherwise they are out of range!
		return coords;
	}

    // Do we want to rotate?
    if (_rotate)
    {
        int16_t temp_x = virt_x;
        virt_x = virt_y;
        virt_y = virtualResY - 1 - temp_x;
    }

	int row  = (virt_y / panelResY); // 0 indexed
	switch(panel_chain_type)
	{
			
		case (CHAIN_TOP_RIGHT_DOWN): 
		{
            if ( (row % 2) == 1 ) 
            { 	// upside down panel

                //Serial.printf("Condition 1, row %d ", row);

                // refersed for the row
                coords.x = dmaResX - virt_x - (row*virtualResX);			

                // y co-ord inverted within the panel
                coords.y = panelResY - 1 - (virt_y % panelResY);	
            

            }
            else
            { 
                //Serial.printf("Condition 2, row %d ", row);

                coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
                coords.y =  virt_y % panelResY;	
                        
            }

		}
			break;	


		case (CHAIN_TOP_LEFT_DOWN): // OK -> modulus opposite of CHAIN_TOP_RIGHT_DOWN
		{
			if ( (row % 2) == 0 ) 
			{ 	// refersed panel 

                //Serial.printf("Condition 1, row %d ", row);
				coords.x = dmaResX - virt_x - (row*virtualResX);			

				// y co-ord inverted within the panel
				coords.y = panelResY - 1 - (virt_y % panelResY);	

			}
			else
			{ 
                //Serial.printf("Condition 2, row %d ", row);
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
			    coords.y =  virt_y % panelResY;	
						
			}

		}
			break;	            
			
			


		case (CHAIN_BOTTOM_LEFT_UP): 	// 
		{
            row = vmodule_rows - row - 1;
		
			if ( (row % 2) == 1 ) 
			{ 	
                // Serial.printf("Condition 1, row %d ", row);
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
				coords.y = virt_y % panelResY;	
 
			}
			else
			{  // inverted panel                     

                // Serial.printf("Condition 2, row %d ", row);
				coords.x =  dmaResX - (row*virtualResX) - virt_x;         
				coords.y = panelResY - 1 - (virt_y % panelResY);
			}
			
		}
			break;
	
		case (CHAIN_BOTTOM_RIGHT_UP): 	// OK -> modulus opposite of CHAIN_BOTTOM_LEFT_UP
		{
            row = vmodule_rows - row - 1;
		
			if ( (row % 2) == 0 ) 
			{ 	// right side up

                // Serial.printf("Condition 1, row %d ", row);
				// refersed for the row
				coords.x =  ((vmodule_rows - (row+1))*virtualResX)+virt_x;
				coords.y = virt_y % panelResY;	
 
			}
			else
			{  // inverted panel                     

                // Serial.printf("Condition 2, row %d ", row);
				coords.x =  dmaResX - (row*virtualResX) - virt_x;         
				coords.y = panelResY - 1 - (virt_y % panelResY);
			}
			
		}
			break;
						

		default:
            return coords;
			break;
			
	} // end switch
	


  /* START: Pixel remapping AGAIN to convert TWO parallel scanline output that the
   *        the underlying hardware library is designed for (because
   *        there's only 2 x RGB pins... and convert this to 1/4 or something
   */
  if (panel_scan_rate == FOUR_SCAN_32PX_HIGH)
  {
    /* Convert Real World 'VirtualMatrixPanel' co-ordinates (i.e. Real World pixel you're looking at
       on the panel or chain of panels, per the chaining configuration) to a 1/8 panels
       double 'stretched' and 'squished' coordinates which is what needs to be sent from the
       DMA buffer.

       Note: Look at the FourScanPanel example code and you'll see that the DMA buffer is setup
       as if the panel is 2 * W and 0.5 * H !
    */

    if ((virt_y & 8) == 0)
    {
      coords.x += ((coords.x / panelResX) + 1) * panelResX; // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }
    else
    {
      coords.x += (coords.x / panelResX) * panelResX; // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }

    // http://cpp.sh/4ak5u
    // Real number of DMA y rows is half reality
    // coords.y = (y / 16)*8 + (y & 0b00000111);
    coords.y = (virt_y >> 4) * 8 + (virt_y & 0b00000111);

  }
  else if (panel_scan_rate == FOUR_SCAN_16PX_HIGH)
  {
    if ((virt_y & 8) == 0)
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4) + 1); // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }
    else
    {
      coords.x += (panelResX >> 2) * (((coords.x & 0xFFF0) >> 4)); // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
    }

    if (virt_y < 32)
      coords.y = (virt_y >> 4) * 8 + (virt_y & 0b00000111);
    else
    {
      coords.y = ((virt_y - 32) >> 4) * 8 + (virt_y & 0b00000111);
      coords.x += 256;
    }
  }

  return coords;
}

inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, uint16_t color)
{ // adafruit virtual void override
  getCoords(x, y);
  // Serial.printf("Requested virtual x,y coord (%d, %d), got phyical chain coord of (%d,%d)\n", x,y, coords.x, coords.y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void VirtualMatrixPanel::fillScreen(uint16_t color)
{ // adafruit virtual void override
  this->display->fillScreen(color);
}

inline void VirtualMatrixPanel::fillScreenRGB888(uint8_t r, uint8_t g, uint8_t b)
{
  this->display->fillScreenRGB888(r, g, b);
}

inline void VirtualMatrixPanel::drawPixelRGB888(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b)
{
  getCoords(x, y);
  this->display->drawPixelRGB888(coords.x, coords.y, r, g, b);
}

#ifdef USE_GFX_ROOT
// Support for CRGB values provided via FastLED
inline void VirtualMatrixPanel::drawPixel(int16_t x, int16_t y, CRGB color)
{
  getCoords(x, y);
  this->display->drawPixel(coords.x, coords.y, color);
}

inline void VirtualMatrixPanel::fillScreen(CRGB color)
{
  this->display->fillScreen(color);
}
#endif

inline void VirtualMatrixPanel::setRotate(bool rotate)
{
  _rotate = rotate;

#ifndef NO_GFX
  // We don't support rotation by degrees.
  if (rotate)
  {
    setRotation(1);
  }
  else
  {
    setRotation(0);
  }
#endif
}

inline void VirtualMatrixPanel::setPhysicalPanelScanRate(PANEL_SCAN_RATE rate)
{
  panel_scan_rate = rate;
}

#ifndef NO_GFX
inline void VirtualMatrixPanel::drawDisplayTest()
{
  this->display->setFont(&FreeSansBold12pt7b);
  this->display->setTextColor(this->display->color565(255, 255, 0));
  this->display->setTextSize(1);

  for (int panel = 0; panel < vmodule_cols * vmodule_rows; panel++)
  {
    int top_left_x = (panel == 0) ? 0 : (panel * panelResX);
    this->display->drawRect(top_left_x, 0, panelResX, panelResY, this->display->color565(0, 255, 0));
    this->display->setCursor( (panel * panelResX)+2, panelResY - 4);
    this->display->print((vmodule_cols * vmodule_rows) - panel);
  }
}
#endif

/*
// need to recreate this one, as it wouldn't work to just map where it starts.
inline void VirtualMatrixPanel::drawIcon (int *ico, int16_t x, int16_t y, int16_t icon_cols, int16_t icon_rows) {
  int i, j;
  for (i = 0; i < icon_rows; i++) {
    for (j = 0; j < icon_cols; j++) {
      // This is a call to this libraries version of drawPixel
      // which will map each pixel, which is what we want.
      //drawPixelRGB565 (x + j, y + i, ico[i * module_cols + j]);
    drawPixel (x + j, y + i, ico[i * icon_cols + j]);
    }
  }
}
*/

#endif
