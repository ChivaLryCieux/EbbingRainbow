// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hall/UI/SHallMainOverlay.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/DateTime.h"

#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Rendering/SlateRenderTransform.h"

void SHallMainOverlay::Construct(const FArguments& InArgs)
{
	// -----------------------------------------------------------------
	// 样式画刷初始化 (深色半透明亚克力 / 科技高对比度)
	// -----------------------------------------------------------------

	// 终端主磁贴底座
	TerminalBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TerminalBgBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.06f, 0.1f, 0.88f));
	TerminalBgBrush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
	TerminalBgBrush.OutlineSettings.Width = 1.5f;
	TerminalBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.25f, 0.45f, 0.8f, 0.75f));

	// 编队 / 干员磁贴底座
	SquadsBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	SquadsBgBrush.TintColor = FSlateColor(FLinearColor(0.05f, 0.07f, 0.11f, 0.85f));
	SquadsBgBrush.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
	SquadsBgBrush.OutlineSettings.Width = 1.2f;
	SquadsBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.2f, 0.35f, 0.6f, 0.6f));

	// 采购中心鲜亮橙红底座
	StoreBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	StoreBgBrush.TintColor = FSlateColor(FLinearColor(0.85f, 0.28f, 0.15f, 0.95f));
	StoreBgBrush.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
	StoreBgBrush.OutlineSettings.Width = 1.0f;
	StoreBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(1.0f, 0.65f, 0.4f, 0.8f));

	// 公开招募底座
	RecruitBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	RecruitBgBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.08f, 0.12f, 0.9f));
	RecruitBgBrush.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
	RecruitBgBrush.OutlineSettings.Width = 1.2f;
	RecruitBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.8f, 0.35f, 0.2f, 0.7f));

	// 基础小磁贴底座
	MiniTileBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	MiniTileBgBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.85f));
	MiniTileBgBrush.OutlineSettings.CornerRadii = FVector4(6.0f, 6.0f, 6.0f, 6.0f);
	MiniTileBgBrush.OutlineSettings.Width = 1.0f;
	MiniTileBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.2f, 0.25f, 0.35f, 0.5f));

	// 顶部条胶囊底座
	TopBarPillBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	TopBarPillBrush.TintColor = FSlateColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.75f));
	TopBarPillBrush.OutlineSettings.CornerRadii = FVector4(14.0f, 14.0f, 14.0f, 14.0f);
	TopBarPillBrush.OutlineSettings.Width = 1.0f;
	TopBarPillBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.3f, 0.35f, 0.45f, 0.5f));

	// 货币资源条底座
	CurrencyChipBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	CurrencyChipBrush.TintColor = FSlateColor(FLinearColor(0.06f, 0.08f, 0.12f, 0.85f));
	CurrencyChipBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
	CurrencyChipBrush.OutlineSettings.Width = 1.2f;
	CurrencyChipBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.4f, 0.5f, 0.7f, 0.7f));

	// 弹窗底座
	ModalBgBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	ModalBgBrush.TintColor = FSlateColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.96f));
	ModalBgBrush.OutlineSettings.CornerRadii = FVector4(12.0f, 12.0f, 12.0f, 12.0f);
	ModalBgBrush.OutlineSettings.Width = 2.0f;
	ModalBgBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.2f, 0.65f, 1.0f, 0.9f));

	// 通用暗色磨砂玻璃底座
	DarkGlassBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	DarkGlassBrush.TintColor = FSlateColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.82f));
	DarkGlassBrush.OutlineSettings.CornerRadii = FVector4(8.0f, 8.0f, 8.0f, 8.0f);
	DarkGlassBrush.OutlineSettings.Width = 1.0f;
	DarkGlassBrush.OutlineSettings.Color = FSlateColor(FLinearColor(0.2f, 0.3f, 0.45f, 0.5f));

	ChildSlot
	[
		SAssignNew(RootOverlay, SOverlay)

		// =================================================================
		// 主界面层 (在观景模式下可平滑隐藏)
		// =================================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(MainDashboardContainer, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
			.Padding(0.0f)
			[
				SNew(SOverlay)

				// -------------------------------------------------------------
				// 1. 顶部状态栏 (左侧系统按钮 + 右侧日期走字与三合一资源条)
				// -------------------------------------------------------------
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				.Padding(28.0f, 18.0f, 28.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					// 顶部左侧: 系统快捷功能按钮组 (设置、公告、邮件、签到)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(&TopBarPillBrush)
						.Padding(FMargin(14.0f, 6.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("⚙ 设置")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor::White)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("⚠ 公告")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.3f))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("✉ 邮件 [1]")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor(0.3f, 0.85f, 1.0f))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("📅 战备日历")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]

					// 顶部右侧: 动态时间与货币芯片
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)

						// 实时时钟走字
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(0.0f, 0.0f, 18.0f, 0.0f))
						[
							SAssignNew(TimeTextBlock, STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor(0.85f, 0.9f, 0.95f, 0.9f))
						]

						// 资源 1: 信用点 / 龙门币 (858,597)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(6.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&CurrencyChipBrush)
							.Padding(FMargin(10.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("[龙] 858,597 ⊕")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.ColorAndOpacity(FLinearColor(0.3f, 0.8f, 1.0f))
								]
							]
						]

						// 资源 2: 合成玉 / 量子结晶 (11,035)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(6.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&CurrencyChipBrush)
							.Padding(FMargin(10.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("[玉] 11,035 ⊕")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.ColorAndOpacity(FLinearColor(0.95f, 0.35f, 0.35f))
								]
							]
						]

						// 资源 3: 至纯源石 / 核心 (172)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(6.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&CurrencyChipBrush)
							.Padding(FMargin(10.0f, 4.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth()
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("[石] 172 ⊕")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
									.ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.2f))
								]
							]
						]

						// 退出游戏小按钮
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
						[
							SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.OnClicked(this, &SHallMainOverlay::HandleQuitClicked)
							[
								SNew(SBorder)
								.BorderImage(&TopBarPillBrush)
								.Padding(FMargin(12.0f, 4.0f))
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("退出游戏")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
									.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.75f))
								]
							]
						]
					]
				]

				// -------------------------------------------------------------
				// 2. 左侧空间站背景视口装饰 (指挥官档案与全屏观景按钮)
				// -------------------------------------------------------------
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(35.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)

					// 指挥官等级大圆徽章与名字
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)

						// 等级圆形胶囊
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBorder)
							.BorderImage(&TopBarPillBrush)
							.Padding(FMargin(12.0f, 8.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("95")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
									.ColorAndOpacity(FLinearColor::White)
								]
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("LV")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									.ColorAndOpacity(FLinearColor(0.6f, 0.7f, 0.85f))
								]
							]
						]

						// 名字与识别码 ID
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(12.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("弦智客")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
								.ColorAndOpacity(FLinearColor::White)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("ID: 94047391 · 轨道驻防舰队")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
								.ColorAndOpacity(FLinearColor(0.65f, 0.75f, 0.85f, 0.8f))
							]
						]
					]

					// 交互药丸按钮: [👁 观景模式(隐藏UI)] 与 [🔄 切换视角]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.OnClicked(this, &SHallMainOverlay::HandleEyeToggleClicked)
							[
								SNew(SBorder)
								.BorderImage(&TopBarPillBrush)
								.Padding(FMargin(12.0f, 5.0f))
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("👁 观景模式 (隐藏UI)")))
									.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
									.ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.0f))
								]
							]
						]
					]
				]

				// 左侧底部: BREAKING NEWS 空间站战况条与好友档案
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Bottom)
				.Padding(35.0f, 0.0f, 0.0f, 30.0f)
				[
					SNew(SHorizontalBox)

					// 战备通报小卡片
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBorder)
						.BorderImage(&DarkGlassBrush)
						.Padding(FMargin(14.0f, 8.0f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("● BREAKING NEWS · 空间站防卫圈")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								.ColorAndOpacity(FLinearColor(1.0f, 0.35f, 0.35f))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("近地轨道量子折叠异常，4人特勤队战备待命中")))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
								.ColorAndOpacity(FLinearColor(0.8f, 0.85f, 0.9f, 0.85f))
							]
						]
					]

					// 好友与档案快捷按钮
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(12.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage(&DarkGlassBrush)
						.Padding(FMargin(12.0f, 8.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("好友")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor::White)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("档案")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FLinearColor(1.0f, 0.65f, 0.3f))
							]
						]
					]
				]

				// -------------------------------------------------------------
				// 3. 右侧明日方舟经典磁贴仪表盘 (Terminal / Squads / Store ...)
				// -------------------------------------------------------------
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 40.0f, 32.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(530.0f)
					[
						SNew(SVerticalBox)

						// ----------------- 第 1 行: 核心磁贴 [Terminal 战术终端] -----------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.OnClicked(this, &SHallMainOverlay::HandleTerminalClicked)
							[
								SNew(SBorder)
								.BorderImage(&TerminalBgBrush)
								.Padding(FMargin(16.0f, 12.0f))
								[
									SNew(SHorizontalBox)

									// 左侧: 理智/能量环形状态
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SBorder)
										.BorderImage(&TopBarPillBrush)
										.Padding(FMargin(14.0f, 10.0f))
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("5")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
												.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.2f))
											]
											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("SANITY 205")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
												.ColorAndOpacity(FLinearColor(0.85f, 0.45f, 0.2f))
											]
										]
									]

									// 中间: Terminal 标题与状态
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									.Padding(18.0f, 0.0f, 0.0f, 0.0f)
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth()
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Terminal")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
												.ColorAndOpacity(FLinearColor::White)
											]
											+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 4.0f, 0.0f, 0.0f)
											[
												SNew(SBorder)
												.BorderImage(&TopBarPillBrush)
												.Padding(FMargin(6.0f, 1.0f))
												[
													SNew(STextBlock)
													.Text(FText::FromString(TEXT("RP 战术终端")))
													.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
													.ColorAndOpacity(FLinearColor(0.9f, 0.3f, 0.2f))
												]
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("● 当前 · 全部完成 (ALL CLEAR)")))
											.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
											.ColorAndOpacity(FLinearColor(0.5f, 0.65f, 0.8f))
										]
									]

									// 右侧: [作战出击 / START SORTIE] 醒目大按钮
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SBorder)
										.BorderImage(&StoreBgBrush)
										.Padding(FMargin(18.0f, 14.0f))
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("作战出击")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
												.ColorAndOpacity(FLinearColor::White)
											]
											+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("START SORTIE >>")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
												.ColorAndOpacity(FLinearColor(1.0f, 0.9f, 0.5f))
											]
										]
									]
								]
							]
						]

						// ----------------- 第 2 行: [Squads 编队] & [Operator 干员] -----------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)

							// 磁贴: [Squads 编队]
							+ SHorizontalBox::Slot()
							.FillWidth(0.5f)
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleSquadsClicked)
								[
									SNew(SBorder)
									.BorderImage(&SquadsBgBrush)
									.Padding(FMargin(14.0f, 10.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth()
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Squads")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
												.ColorAndOpacity(FLinearColor::White)
											]
											+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Pioneer Project ··")))
												.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
												.ColorAndOpacity(FLinearColor(0.45f, 0.55f, 0.7f))
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("编队 :· (4/4 特勤就绪)")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
											.ColorAndOpacity(FLinearColor(0.35f, 0.85f, 1.0f))
										]
									]
								]
							]

							// 磁贴: [Operator 干员]
							+ SHorizontalBox::Slot()
							.FillWidth(0.5f)
							.Padding(6.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleOperatorClicked)
								[
									SNew(SBorder)
									.BorderImage(&SquadsBgBrush)
									.Padding(FMargin(14.0f, 10.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth()
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Operator")))
												.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
												.ColorAndOpacity(FLinearColor::White)
											]
											+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Pioneer Project ··")))
												.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
												.ColorAndOpacity(FLinearColor(0.45f, 0.55f, 0.7f))
											]
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("干员与卡组 :·")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
											.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.35f))
										]
									]
								]
							]
						]

						// ----------------- 第 3 行: [Store 采购中心] & [Recruit 公开招募] -----------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)

							// 磁贴: [Store 采购中心]
							+ SHorizontalBox::Slot()
							.FillWidth(0.45f)
							.Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleStoreClicked)
								[
									SNew(SBorder)
									.BorderImage(&StoreBgBrush)
									.Padding(FMargin(14.0f, 12.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("STORE")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
											.ColorAndOpacity(FLinearColor::White)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("采购中心")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
											.ColorAndOpacity(FLinearColor(1.0f, 0.95f, 0.85f))
										]
									]
								]
							]

							// 磁贴: [Recruitment 公开招募]
							+ SHorizontalBox::Slot()
							.FillWidth(0.55f)
							.Padding(6.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleRecruitClicked)
								[
									SNew(SBorder)
									.BorderImage(&RecruitBgBrush)
									.Padding(FMargin(14.0f, 12.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Recruitment · 公开招募")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
											.ColorAndOpacity(FLinearColor(1.0f, 0.55f, 0.35f))
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("特勤寻访 · 招募中 09:00:00")))
											.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
											.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.9f))
										]
									]
								]
							]
						]

						// ----------------- 第 4 行: [Mission 任务] / [Base 基建] / [Depot 仓库] -----------------
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							// 磁贴: [Mission 任务]
							+ SHorizontalBox::Slot()
							.FillWidth(0.35f)
							.Padding(0.0f, 0.0f, 5.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleMissionClicked)
								[
									SNew(SBorder)
									.BorderImage(&MiniTileBgBrush)
									.Padding(FMargin(12.0f, 10.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Mission")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
											.ColorAndOpacity(FLinearColor::White)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("任务 :·")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
											.ColorAndOpacity(FLinearColor(0.6f, 0.75f, 0.9f))
										]
									]
								]
							]

							// 磁贴: [Base 空间基建]
							+ SHorizontalBox::Slot()
							.FillWidth(0.35f)
							.Padding(5.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleBaseClicked)
								[
									SNew(SBorder)
									.BorderImage(&MiniTileBgBrush)
									.Padding(FMargin(12.0f, 10.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Base")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
											.ColorAndOpacity(FLinearColor::White)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("空间基建 :·")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
											.ColorAndOpacity(FLinearColor(0.55f, 0.9f, 0.65f))
										]
									]
								]
							]

							// 磁贴: [Depot 战备仓库]
							+ SHorizontalBox::Slot()
							.FillWidth(0.3f)
							.Padding(5.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(FCoreStyle::Get(), "NoBorder")
								.OnClicked(this, &SHallMainOverlay::HandleDepotClicked)
								[
									SNew(SBorder)
									.BorderImage(&MiniTileBgBrush)
									.Padding(FMargin(12.0f, 10.0f))
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot().AutoHeight()
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Depot")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
											.ColorAndOpacity(FLinearColor::White)
										]
										+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("仓库 (11天)")))
											.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
											.ColorAndOpacity(FLinearColor(0.85f, 0.7f, 0.35f))
										]
									]
								]
							]
						]
					]
				]
			]
		]

		// =================================================================
		// 编队详情模态弹窗 (Squads Modal)
		// =================================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(SquadsModalBorder, SBorder)
			.BorderImage(&ModalBgBrush)
			.Padding(FMargin(28.0f, 22.0f))
			.Visibility(EVisibility::Hidden)
			[
				SNew(SBox)
				.WidthOverride(650.0f)
				[
					SNew(SVerticalBox)

					// 弹窗标题栏
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("— 特勤战备编队 (SQUADS DEPLOYMENT) —")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
							.ColorAndOpacity(FLinearColor(0.2f, 0.8f, 1.0f))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right)
						[
							SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.OnClicked_Lambda([this]() {
								ToggleSquadsModal();
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("[关闭 X]")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
								.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f))
							]
						]
					]

					// 4位英雄编队属性一览
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 20.0f)
					[
						SNew(SHorizontalBox)

						// 英雄 1: 先锋
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(FMargin(4.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&DarkGlassBrush)
							.Padding(FMargin(10.0f, 12.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("01 · 先锋"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FLinearColor(0.3f, 0.8f, 1.0f))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("战术队长\nHP: 100\nAP: 20\nSPD: 14\n斩击/射击"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor::White)
								]
							]
						]

						// 英雄 2: 突击
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(FMargin(4.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&DarkGlassBrush)
							.Padding(FMargin(10.0f, 12.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("02 · 突击"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FLinearColor(1.0f, 0.45f, 0.2f))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("破阵先锋\nHP: 90\nAP: 20\nSPD: 12\n强袭重斩"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor::White)
								]
							]
						]

						// 英雄 3: 狙击
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(FMargin(4.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&DarkGlassBrush)
							.Padding(FMargin(10.0f, 12.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("03 · 狙击"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FLinearColor(0.3f, 0.95f, 0.4f))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("轨道射手\nHP: 80\nAP: 20\nSPD: 10\n远程狙击"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor::White)
								]
							]
						]

						// 英雄 4: 支援
						+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(FMargin(4.0f, 0.0f))
						[
							SNew(SBorder)
							.BorderImage(&DarkGlassBrush)
							.Padding(FMargin(10.0f, 12.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("04 · 支援"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(FLinearColor(0.85f, 0.45f, 1.0f))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock).Text(FText::FromString(TEXT("战术协调\nHP: 85\nAP: 20\nSPD: 11\n战术充能"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(FLinearColor::White)
								]
							]
						]
					]

					// 底部出击大按钮
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.OnClicked_Lambda([this]() {
							LaunchBattleLevel();
							return FReply::Handled();
						})
						[
							SNew(SBorder)
							.BorderImage(&StoreBgBrush)
							.Padding(FMargin(40.0f, 12.0f))
							[
								SNew(STextBlock)
								.Text(FText::FromString(TEXT("★ 确认出击 (START SORTIE) ★")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
								.ColorAndOpacity(FLinearColor::White)
							]
						]
					]
				]
			]
		]

		// =================================================================
		// 全屏观景模式提示框 (Zen Hint)
		// =================================================================
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 25.0f)
		[
			SAssignNew(ZenHintBorder, SBorder)
			.BorderImage(&TopBarPillBrush)
			.Padding(FMargin(20.0f, 8.0f))
			.Visibility(EVisibility::Hidden)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("— 全屏观景模式已激活 · 点击屏幕任意位置恢复界面 —")))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.0f, 0.85f))
			]
		]
	];
}

void SHallMainOverlay::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 实时刷新顶部时间
	if (TimeTextBlock.IsValid())
	{
		const FString TimeStr = FDateTime::Now().ToString(TEXT("%Y/%m/%d %H:%M:%S"));
		TimeTextBlock->SetText(FText::FromString(FString::Printf(TEXT("▰ 空间站时区: %s"), *TimeStr)));
	}
}

FReply SHallMainOverlay::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// 观景模式下点击任意位置恢复 UI
	if (bZenMode)
	{
		ToggleZenMode();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SHallMainOverlay::ToggleZenMode()
{
	bZenMode = !bZenMode;
	if (MainDashboardContainer.IsValid())
	{
		MainDashboardContainer->SetVisibility(bZenMode ? EVisibility::Hidden : EVisibility::Visible);
	}
	if (ZenHintBorder.IsValid())
	{
		ZenHintBorder->SetVisibility(bZenMode ? EVisibility::Visible : EVisibility::Hidden);
	}
}

void SHallMainOverlay::ToggleSquadsModal()
{
	bSquadsModalOpen = !bSquadsModalOpen;
	if (SquadsModalBorder.IsValid())
	{
		SquadsModalBorder->SetVisibility(bSquadsModalOpen ? EVisibility::Visible : EVisibility::Hidden);
	}
}

void SHallMainOverlay::LaunchBattleLevel()
{
	if (UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr)
	{
		UGameplayStatics::OpenLevel(World, FName("/Game/Battle/Maps/L_Battle?game=/Script/EbbingRainbow.BattleGameMode"));
	}
}

FReply SHallMainOverlay::HandleTerminalClicked()
{
	LaunchBattleLevel();
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleSquadsClicked()
{
	ToggleSquadsModal();
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleOperatorClicked()
{
	ToggleSquadsModal();
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleStoreClicked()
{
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleRecruitClicked()
{
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleMissionClicked()
{
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleBaseClicked()
{
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleDepotClicked()
{
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleEyeToggleClicked()
{
	ToggleZenMode();
	return FReply::Handled();
}

FReply SHallMainOverlay::HandleQuitClicked()
{
	if (UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
			return FReply::Handled();
		}
	}
	return FReply::Handled();
}
