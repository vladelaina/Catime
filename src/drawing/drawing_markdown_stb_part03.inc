                if (h > lineMaxHeight) lineMaxHeight = h;

                int asc = GetLineHeightFromMetric(baseAscent, scale); // Approximation
                if (asc > maxAscent) maxAscent = asc;
            }

            int currentX = blockLeftX;  // All lines start from same left edge
            // Baseline align: Y + maxAscent
            int baselineY = AddMarkdownIntClamped(currentY, maxAscent);

            // Check for horizontal rule (─── = \x2500\x2500\x2500)
            if (i - currentLineStart >= 3 &&
                text[currentLineStart] == L'\x2500' &&
                text[currentLineStart + 1] == L'\x2500' &&
                text[currentLineStart + 2] == L'\x2500') {
                // Draw horizontal line within text block area only
                long long lineY64 = (long long)currentY + (long long)(lineMaxHeight / 2);
                DWORD* pixels = (DWORD*)bits;

                long long hrLeft = (long long)blockLeftX;
                int hrWidth = maxLineWidth;

                if (lineY64 >= 0 && lineY64 < (long long)height && hrWidth > 0) {
                    int lineY = (int)lineY64;
                    int drawStart = 0;
                    int drawEnd = 0;
                    if (CalculateVisibleSpan(hrLeft, hrWidth, width, &drawStart, &drawEnd)) {
                        const GradientInfo* hrGradInfo = frameGradientInfo;
                        long long animOffsetFixed = (hrGradInfo &&
                                                     hrGradInfo->palette &&
                                                     hrGradInfo->paletteCount > 2)
                            ? ((long long)timeOffset * MARKDOWN_GRADIENT_FIXED_ONE) /
                              (long long)(GRADIENT_LUT_SIZE * 2)
                            : 0;
                        long long gradientStep = MarkdownGradientStepFixed(hrWidth);
                        long long gradientPosition = MarkdownGradientPositionFixed(drawStart,
                                                                                   hrWidth,
                                                                                   animOffsetFixed);
                        DWORD* row = pixels + (size_t)lineY * (size_t)width;

                        for (int offset = drawStart; offset < drawEnd; offset++) {
                            int x = (int)(hrLeft + (long long)offset);
                            DWORD lineColor;
                            if (hrGradInfo) {
                                COLORREF sample = SampleGlobalGradientFixed(hrGradInfo, gradientPosition);
                                lineColor = 0xFF000000 |
                                            (GetRValue(sample) << 16) |
                                            (GetGValue(sample) << 8) |
                                            GetBValue(sample);
                                AdvanceMarkdownGradientFixed(&gradientPosition, gradientStep);
                            } else {
                                lineColor = 0xFF000000 |
                                            (GetRValue(color) << 16) |
                                            (GetGValue(color) << 8) |
                                            GetBValue(color);
                            }
                            row[x] = lineColor;
                        }
                    }
                }
                currentY = AddMarkdownIntClamped(currentY, lineMaxHeight);
                currentLineStart = i + 1;
                continue;
            }

            // Check if this line is inside a blockquote
            int currentLineStartPos = ClampMarkdownPos(currentLineStart);
            while (curBlockquoteIdx < blockquoteCount &&
                   currentLineStartPos >= blockquotes[curBlockquoteIdx].endPos) {
                curBlockquoteIdx++;
            }

            BlockquoteAlertType activeAlertType = BLOCKQUOTE_NORMAL;
            BOOL inBlockquote = FALSE;
            if (curBlockquoteIdx < blockquoteCount &&
                currentLineStartPos >= blockquotes[curBlockquoteIdx].startPos) {
                inBlockquote = TRUE;
                activeAlertType = blockquotes[curBlockquoteIdx].alertType;
            }

            // Draw left colored bar for alert blockquotes only (GitHub style)
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                COLORREF barColor = GetAlertColor(activeAlertType);
                DWORD barColorDW = 0xFF000000 | (GetRValue(barColor) << 16) |
                                   (GetGValue(barColor) << 8) | GetBValue(barColor);
                DWORD* pixels = (DWORD*)bits;

                int barX = blockLeftX - 8;  // 8 pixels left of text
                int barWidth = 3;           // 3 pixels wide

                int firstY = 0;
                int lastY = 0;
                int firstX = 0;
                int lastX = 0;
                if (CalculateVisibleSpan(currentY, lineMaxHeight, height, &firstY, &lastY) &&
                    CalculateVisibleSpan(barX, barWidth, width, &firstX, &lastX)) {
                    for (int yOffset = firstY; yOffset < lastY; yOffset++) {
                        int y = (int)((long long)currentY + (long long)yOffset);
                        DWORD* row = pixels + (size_t)y * (size_t)width;
                        for (int xOffset = firstX; xOffset < lastX; xOffset++) {
                            int x = (int)((long long)barX + (long long)xOffset);
                            row[x] = barColorDW;
                        }
                    }
                }
            }

            // Check if this is an alert title line (first line with "NOTE:", etc.)
            BOOL isAlertTitleLine = FALSE;
            if (inBlockquote && activeAlertType != BLOCKQUOTE_NORMAL) {
                const wchar_t* lineText = &text[currentLineStart];
                if (StartsWithLiteralW(lineText, L"NOTE:") ||
                    StartsWithLiteralW(lineText, L"TIP:") ||
                    StartsWithLiteralW(lineText, L"IMPORTANT:") ||
                    StartsWithLiteralW(lineText, L"WARNING:") ||
                    StartsWithLiteralW(lineText, L"CAUTION:")) {
                    isAlertTitleLine = TRUE;
                }
            }

            // Check if this is a completed todo line (starts with ■)
            BOOL isCompletedTodo = (text[currentLineStart] == L'\x25A0');

            // 2. Render this line
            for (size_t j = currentLineStart; j < i; j++) {
                if (text[j] == L'\r') continue;

                // Determine styles
                float scale = baseScale;
                float fallbackScale = fallbackBaseScale;
                COLORREF drawColor = color;
                int renderPos = ClampMarkdownPos(j);

                // Heading
                while (curHeadingIdx < headingCount && renderPos >= headings[curHeadingIdx].endPos) curHeadingIdx++;
                if (curHeadingIdx < headingCount && renderPos >= headings[curHeadingIdx].startPos) {
                    scale = GetScaleForHeading(headings[curHeadingIdx].level, baseScale);
                    if (fallbackLoaded) fallbackScale = GetScaleForHeading(headings[curHeadingIdx].level, fallbackBaseScale);
                }

                // Apply alert color only to title line (NOTE:, TIP:, etc.)
                if (isAlertTitleLine) {
                    drawColor = GetAlertColor(activeAlertType);
                }

                // Link - track region for click detection
                BOOL inLink = FALSE;
                int activeLinkIdx = -1;
                while (curLinkIdx < linkCount && renderPos >= links[curLinkIdx].endPos) curLinkIdx++;
                if (curLinkIdx < linkCount && renderPos >= links[curLinkIdx].startPos) {
                    drawColor = RGB(0, 175, 255); // Link color #00AFFF
                    inLink = TRUE;
                    activeLinkIdx = curLinkIdx;

                    /* Update link rect for first char */
                    if (renderPos == links[curLinkIdx].startPos) {
                        links[curLinkIdx].linkRect.left = currentX;
                        links[curLinkIdx].linkRect.top = currentY;
                        links[curLinkIdx].linkRect.bottom = AddMarkdownIntClamped(currentY, lineMaxHeight);
                    }
                    links[curLinkIdx].linkRect.right = currentX;
                }

                // Style handling
                BOOL isBold = isAlertTitleLine;  // Alert title is bold
                BOOL isItalic = FALSE;
                BOOL isStrikethrough = FALSE;

                // Apply strikethrough for completed todo (skip checkbox symbol itself)
                if (isCompletedTodo && renderPos > currentLineStartPos) {
                    isStrikethrough = TRUE;
                }

                while (curStyleIdx < styleCount && renderPos >= styles[curStyleIdx].endPos) curStyleIdx++;
                if (curStyleIdx < styleCount && renderPos >= styles[curStyleIdx].startPos) {
                    MarkdownStyleType styleType = styles[curStyleIdx].type;
                    if (styleType == STYLE_CODE) {
                        drawColor = RGB(100, 100, 100);
                    } else if (styleType == STYLE_BOLD) {
                        isBold = TRUE;
                    } else if (styleType == STYLE_ITALIC) {
                        isItalic = TRUE;
                    } else if (styleType == STYLE_BOLD_ITALIC) {
                        isBold = TRUE;
                        isItalic = TRUE;
                    } else if (styleType == STYLE_STRIKETHROUGH) {
                        isStrikethrough = TRUE;
                    }
                }

                /* Color tag handling - override drawColor with tag color or gradient */
                BOOL useColorTagGradient = FALSE;
                const MarkdownColorTag* activeColorTag = NULL;
                while (curColorTagIdx < colorTagCount && j >= (size_t)colorTags[curColorTagIdx].endPos) curColorTagIdx++;
                if (curColorTagIdx < colorTagCount && j >= (size_t)colorTags[curColorTagIdx].startPos) {
                    const MarkdownColorTag* tag = &colorTags[curColorTagIdx];
                    if (tag->colorCount == 1) {
                        /* Single color */
                        drawColor = tag->colors[0];
                    } else if (tag->colorCount > 1) {
                        /* Gradient - will use animated rendering */
                        useColorTagGradient = TRUE;
                        activeColorTag = tag;
                    }
                }

                /* Font tag handling - use cached font if specified */
                const stbtt_fontinfo* charFontInfo = fontInfo;
                float charScale = scale;
                while (curFontTagIdx < fontTagCount && j >= (size_t)fontTags[curFontTagIdx].endPos) curFontTagIdx++;
                if (curFontTagIdx < fontTagCount && j >= (size_t)fontTags[curFontTagIdx].startPos) {
                    if (cachedFontTagIdx != curFontTagIdx) {
                        cachedFontTagIdx = curFontTagIdx;
                        cachedFontTagInfo = GetCachedFontSTB(fontTags[curFontTagIdx].fontName);
                        cachedFontTagScale = cachedFontTagInfo ?
                            stbtt_ScaleForPixelHeight(cachedFontTagInfo, (float)(fontSize * fontScale)) :
                            0.0f;
                    }
                    if (cachedFontTagInfo) {
                        charFontInfo = cachedFontTagInfo;
                        charScale = cachedFontTagScale;
                    }
                }

                GlyphMetrics gm;
                /* Use custom font if in font tag, otherwise use default */
                if (charFontInfo != fontInfo) {
                    if (!GetCachedFontCharMetricsSTB(charFontInfo, text[j], charScale, &gm) ||
                        gm.index == 0) {
                        /* Fallback to main font if glyph not found */
                        GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                        charFontInfo = fontInfo;
                        charScale = scale;
                    }
                } else {
                    GetCharMetricsSTB(text[j], (j < i - 1) ? text[j+1] : 0, scale, fallbackScale, &gm);
                }

                if (gm.index != 0 && text[j] != L' ' && text[j] != L'\t') {
                    int w, h, xoff, yoff;
                    unsigned char* bitmap = NULL;

                    const stbtt_fontinfo* glyphFontInfo = fontInfo;
                    float glyphScale = scale;
                    if (charFontInfo != fontInfo && !gm.isFallback) {
                        glyphFontInfo = charFontInfo;
                        glyphScale = charScale;
                    } else if (gm.isFallback) {
                        glyphFontInfo = fallbackFontInfo;
                        glyphScale = fallbackScale;
                    }

                    int visibilityMargin = isItalic ? lineMaxHeight : 0;
                    if (activeEffect != EFFECT_TYPE_NONE) {
                        visibilityMargin = AddMarkdownIntClamped(visibilityMargin, 24);
                    }
                    bitmap = CreateVisibleGlyphBitmapSTB(glyphFontInfo, gm.index,
                                                         glyphScale, glyphScale,
                                                         currentX, baselineY,
                                                         width, height,
