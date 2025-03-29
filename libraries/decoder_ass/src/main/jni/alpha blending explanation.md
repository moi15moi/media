## Libass Alpha Blending

This document explain how the alpha blending [libass use](https://github.com/libass/libass/blob/1b699559025185e34d21a24cac477ca360cb917d/test/test.c#L149-L165) works.

Let's define the following variables:

- $$a\\_source \in [0, 255]$$
- $$a\\_destination \in [0, 255]$$
- $$a\\_result \in [0, 255]$$
- $$color\\_source \in [0, 255]$$
- $$color\\_destination \in [0, 255]$$
- $$color\\_result \in [0, 255]$$

### Standard Blending Formula

The standard alpha blending formulas are:

$$color\\_result = {a\\_source \over 255} \times color\\_source + (1 - {a\\_source \over 255}) \times color\\_destination$$
$$a\\_result = a\\_source + (255 - a\\_source) \times {a\\_destination \over 255}$$

### Issue with Libass Alpha Calculation

However, Libass does **not** directly provide $$a\\_source$$. It is derived using the formula:

$$a\\_source = {libass\\_color\\_alpha * libass\\_bitmap \over 255}$$

### Precision Loss and Correction

This division introduces truncation errors, resulting in a loss of precision. To mitigate this, we avoid the division and redefine:

$$a\\_source = libass\\_color\\_alpha * libass\\_bitmap$$

### Updated Blending Formula

Because of this change, the blending formula for the color result must be adjusted:

$$color\\_result = {a\\_source \over 255 * 255} \times color\\_source + (1 - {a\\_source \over 255 * 255}) \times color\\_destination$$

### Rounding for Better Precision

To improve accuracy and reduce rounding errors, we introduce a rounding term:

$$color\\_result = {a\\_source \over 255 * 255} \times color\\_source + (1 - {a\\_source \over 255 * 255}) \times color\\_destination + {1 \over 2}$$

### Optimized Integer Division Formula

Since we are performing integer division, a more precise formulation is:

$$color\\_result = {a\\_source \times color\\_source + (255 * 255 - a\\_source) \times color\\_destination + 255 * {255 \over 2} \over 255 * 255}$$

This approach minimizes precision loss and ensures more accurate color blending when using Libass.