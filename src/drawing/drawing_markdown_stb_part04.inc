                                                         visibilityMargin,
                                                         &w, &h, &xoff, &yoff);

                    if (bitmap) {
                        float slant = isItalic ? 0.35f : 0.0f;
                        int drawR = GetRValue(drawColor);
                        int drawG = GetGValue(drawColor);
                        int drawB = GetBValue(drawColor);
                        int glyphX = AddMarkdownIntClamped(currentX, xoff);
                        int glyphY = AddMarkdownIntClamped(baselineY, yoff);
                        int glyphXBold = AddMarkdownIntClamped(glyphX, 1);
                        int glyphYBold = AddMarkdownIntClamped(glyphY, 1);

                        /* Use global gradient only if no color tag gradient is active */
                        BOOL useGlobalGradient = (frameGradientInfo && drawColor == color && !useColorTagGradient);

                        if (useGlobalGradient) {
                            if (isItalic) {
                                // Use proper per-row shear for italic with gradient
                                BlendCharBitmapItalicGradientSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, slant, frameGradientInfo, timeOffset, width);
                                if (isBold) {
                                    BlendCharBitmapItalicGradientSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, slant, frameGradientInfo, timeOffset, width);
                                }
                            } else {
                                BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                    activeEffect);
                                if (isBold) {
                                    BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                        activeEffect);
                                    BlendCharBitmapGradientSTBWithInfo(bits, width, height,
                                        glyphX, glyphYBold,
                                        bitmap, w, h, 0, width, frameGradientInfo, timeOffset,
                                        activeEffect);
                                }
                            }
                        } else if (useColorTagGradient && activeColorTag) {
                            /* Color tag gradient with animation */
                            if (isItalic) {
                                BlendCharBitmapColorTagGradientItalicSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, activeColorTag, timeOffset, width, slant);
                                if (isBold) {
                                    BlendCharBitmapColorTagGradientItalicSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, activeColorTag, timeOffset, width, slant);
                                }
                            } else {
                                BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                    glyphX, glyphY,
                                    bitmap, w, h, activeColorTag, timeOffset, width);
                                if (isBold) {
                                    BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                        glyphXBold, glyphY,
                                        bitmap, w, h, activeColorTag, timeOffset, width);
                                    BlendCharBitmapColorTagGradientSTB(bits, width, height,
                                        glyphX, glyphYBold,
                                        bitmap, w, h, activeColorTag, timeOffset, width);
                                }
                            }
                        } else if (isItalic) {
                            // Use proper per-row shear for italic
                            BlendCharBitmapItalicSTB(bits, width, height,
                                glyphX, glyphY,
                                bitmap, w, h,
                                drawR, drawG, drawB, slant);
                            if (isBold) {
                                BlendCharBitmapItalicSTB(bits, width, height,
                                    glyphXBold, glyphY,
                                    bitmap, w, h,
                                    drawR, drawG, drawB, slant);
                            }
                        } else {
                            BlendCharBitmapSTBWithEffect(bits, width, height,
                                glyphX, glyphY,
                                bitmap, w, h,
                                drawR, drawG, drawB,
                                activeEffect, effectTimeOffset);
                            if (isBold) {
                                BlendCharBitmapSTBWithEffect(bits, width, height,
                                    glyphXBold, glyphY,
                                    bitmap, w, h,
                                    drawR, drawG, drawB,
                                    activeEffect, effectTimeOffset);
                                BlendCharBitmapSTBWithEffect(bits, width, height,
                                    glyphX, glyphYBold,
                                    bitmap, w, h,
                                    drawR, drawG, drawB,
                                    activeEffect, effectTimeOffset);
                            }
                        }
                        stbtt_FreeBitmap(bitmap, NULL);

                        // Draw strikethrough line
                        if (isStrikethrough) {
                            int lineY = AddMarkdownIntClamped(baselineY, -(h / 3));  // Position at ~1/3 from baseline
                            DWORD* pixels = (DWORD*)bits;

                            // Get line color from gradient or solid color
                            DWORD lineColor;
                            if (gradientMode != GRADIENT_NONE && drawColor == color) {
                                lineColor = globalStrikethroughLineColor;
                            } else {
                                lineColor = 0xFF000000 | (drawR << 16) | (drawG << 8) | drawB;
                            }

                            // Draw horizontal line through character
                            if (lineY >= 0 && lineY < height && gm.advance > 0) {
                                int firstStrikeX = 0;
                                int lastStrikeX = 0;
                                if (CalculateVisibleSpan(currentX, gm.advance, width,
                                                         &firstStrikeX, &lastStrikeX)) {
                                    DWORD* row = pixels + (size_t)lineY * (size_t)width;
                                    for (int sx = firstStrikeX; sx < lastStrikeX; sx++) {
                                        int x = (int)((long long)currentX + (long long)sx);
                                        row[x] = lineColor;
                                    }
                                }
                            }
                        }
                    }
                    /* Record checkbox region (□ = 0x25A1, ■ = 0x25A0) */
                    if (text[j] == L'\x25A1' || text[j] == L'\x25A0') {
                        /* Use character advance width for click area */
                        RECT cbRect = {
                            currentX, currentY,
                            AddMarkdownIntClamped(currentX, gm.advance),
                            AddMarkdownIntClamped(currentY, lineMaxHeight)
                        };
                        AddCheckboxRegion(&cbRect, checkboxIndex, text[j] == L'\x25A0');
                        checkboxIndex++;
                    }
                }
                currentX = AddMarkdownIntClamped(currentX, gm.advance + gm.kern);

                /* Update link rect right edge after advancing */
                if (inLink && activeLinkIdx >= 0) {
                    links[activeLinkIdx].linkRect.right = currentX;
                }
            }

            currentY = AddMarkdownIntClamped(currentY, lineMaxHeight);
            currentLineStart = i + 1;
        }
    }

    /* Register all link regions for click detection */
    for (int i = 0; i < linkCount; i++) {
        if (links[i].linkUrl && links[i].linkRect.right > links[i].linkRect.left) {
            AddLinkRegion(&links[i].linkRect, links[i].linkUrl);
        }
    }

done:
    EndFontUseSTB();
}
