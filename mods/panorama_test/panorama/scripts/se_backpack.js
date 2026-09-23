"use strict";
// SE port (2026-09-22): 自创"背包"面板（塔科夫式网格）。
//
// 设计要点（用户 2026-09-22 拍板："想逃离塔科夫一样的就行先做个大概"）：
//   * 固定网格容器（12 列 x 8 行），物品按自己的格子尺寸占位，紧密排列
//   * 悬停物品 -> 右侧显示详情（名字 / 稀有度 / 尺寸）
//   * 数据全部来自已有的真物品库（InventoryAPI，81 武器 / 1085 皮肤条目）
//
// 尺寸表 ITEM_SIZE 由 build/_gen_backpack_sizes.ps1 从 se_econ_real.js 的 sub 字段生成：
// CS:GO 的 econ 数据里没有"格子尺寸"，这是本面板自创的展示规则（步枪 5x2 / 狙击 6x2 /
// 冲锋枪 4x2 / 手枪 2x2 / 霰弹 5x2 / 机枪 6x2 / 盾 3x3 / 刀 1x3 / 手雷 1x1 / C4 2x1）。
(function () {
	var COLS = 16;
	var ROWS = 10;
	var CELL = 56;			// 每格边长（像素）
	var GAP = 2;			// 格子内缩，留出网格缝

	// def -> "WxH"，（build/_gen_backpack_sizes.ps1 生成，改数据后重跑该脚本）
	var SIZE_STR = "1:2x2,2:2x2,3:2x2,4:2x2,7:5x2,8:5x2,9:6x2,10:5x2,"
		+ "11:6x2,13:5x2,14:6x2,16:5x2,17:4x2,19:4x2,20:2x2,23:4x2,"
		+ "24:4x2,25:5x2,26:4x2,27:5x2,28:6x2,29:5x2,30:2x2,31:2x2,"
		+ "32:2x2,33:4x2,34:4x2,35:5x2,36:2x2,37:3x3,38:6x2,39:5x2,"
		+ "40:6x2,41:2x2,42:1x3,43:1x1,44:1x1,45:1x1,46:1x1,47:1x1,"
		+ "48:1x1,49:2x1,57:2x2,59:1x3,60:5x2,61:2x2,63:2x2,64:2x2,"
		+ "68:2x2,69:2x2,70:2x2,72:2x2,74:2x2,75:2x2,76:2x2,78:2x2,"
		+ "80:2x2,81:2x2,82:2x2,83:2x2,84:2x2,85:2x2,500:1x3,503:1x3,"
		+ "505:1x3,506:1x3,507:1x3,508:1x3,509:1x3,512:1x3,514:1x3,515:1x3,"
		+ "516:1x3,517:1x3,518:1x3,519:1x3,520:1x3,521:1x3,522:1x3,523:1x3,"
		+ "525:1x3,";

	var ITEM_SIZE = {};
	(function parseSizes() {
		var parts = String(SIZE_STR).split(",");
		for (var i = 0; i < parts.length; ++i) {
			var kv = parts[i].split(":");
			if (kv.length !== 2) { continue; }
			var wh = kv[1].split("x");
			ITEM_SIZE[kv[0]] = [(Number(wh[0]) || 2), (Number(wh[1]) || 2)];
		}
	})();

	// ------------------------------------------------------------------------
	// 背包里放什么
	//
	// 塔科夫式背包要的是"一背包东西"，不是把 CS:GO 的 1000+ 条皮肤目录塞进来 —— 之前那版遍历整个
	// 库存按字母序摆，前 13 件全是 AK 系列就把 96 格填满了（实测 放入 13 / 放不下 1097）。
	// 所以这里只挑一份装备清单（defindex），按 items_game 的槽位各来一把/一个：
	//   主武器 7=AK-47 9=AWP 16=M4A4 60=M4A1-S · 副武器 1=沙鹰 4=格洛克 61=USP-S
	//   近战 42=刀 500=刺刀 · 投掷物 43=闪光 44=手雷 45=烟雾 46=燃烧瓶 · 49=C4
	// 尺寸表 ITEM_SIZE 里每类都有格子占用，摆出来就是塔科夫那种长短不一地占格的样子。
	// ------------------------------------------------------------------------
	var WANTED_DEFS = [ 7, 9, 16, 60, 1, 4, 61, 42, 500, 43, 44, 45, 46, 49 ];

	function itemPaint(id) {
		// "se_store_<def>_<paintkit>"；没写 paintkit 的按 0（无皮肤的那种"基础武器"）算
		var s = String(id === undefined || id === null ? "" : id);
		var parts = s.split("_");
		return parts.length >= 4 ? parts[3] : "0";
	}

	// ------------------------------------------------------------------------
	// 价格
	//
	// 本构建里没有真实价目表：se_session_sim.js 的 StoreAPI.GetStoreItemSalePrice/OriginalPrice 与
	// LoadoutAPI.GetItemGamePrice 都是空桩（注释也写着 "This build has no GC price sheet"）。
	// 所以价格按"武器类别基准价 × 稀有度系数"生成 —— 和上面的 ITEM_SIZE 一样，属于本面板自创的
	// 展示规则，不是零售价；将来接上真经济数据后换成真价即可（换 itemPrice() 一处）。
	// ------------------------------------------------------------------------
	var BASE_PRICE = {
		1: 700,  2: 300,  3: 500,  4: 300,  7: 2700, 8: 3300, 9: 4750, 10: 2050,
		11: 5000, 13: 1800, 14: 5200, 16: 3100, 17: 1050, 19: 2350, 20: 200,
		23: 1250, 24: 1050, 25: 700, 26: 2050, 27: 1500, 28: 1700, 29: 1800,
		30: 500, 31: 200, 32: 200, 33: 1300, 34: 1200, 35: 2100, 36: 500,
		37: 300, 38: 1700, 39: 3000, 40: 1700, 41: 500, 42: 300, 43: 200,
		44: 300, 45: 300, 46: 400, 47: 50, 48: 200, 49: 400, 57: 100, 59: 100,
		60: 3200, 61: 200, 63: 500, 64: 300, 500: 1200, 503: 1200, 505: 1200,
		506: 1200, 507: 1200, 508: 1200, 509: 1200, 512: 1200, 514: 1200, 515: 1200,
	};
	var RARITY_MULT = [1.0, 1.0, 1.6, 2.8, 5.0, 12.0, 30.0, 120.0];	// 消费级..★ 金
	var DEFAULT_BASE_PRICE = 500;

	function itemRarity(id) {
		var r = 0;
		try { r = Number(InventoryAPI.GetItemAttributeValue(id, "rarity")); } catch (e) { r = 0; }
		if (!r) {
			// sim 没给 rarity 时用颜色反查（和 itemRarityName 的兜底同一套表）
			var c = itemRarityColor(id).toLowerCase();
			if (c === "#b0c3d9") { r = 1; }
			else if (c === "#5e98d9") { r = 2; }
			else if (c === "#4b69ff") { r = 3; }
			else if (c === "#8847ff") { r = 4; }
			else if (c === "#d32ce6") { r = 5; }
			else if (c === "#eb4b4b") { r = 6; }
			else if (c === "#e4ae39") { r = 7; }
		}
		return Math.max(0, Math.min(7, r));
	}

	function itemPrice(id) {
		var def = itemDef(id);
		var base = BASE_PRICE.hasOwnProperty(def) ? BASE_PRICE[def] : DEFAULT_BASE_PRICE;
		return Math.round(base * RARITY_MULT[itemRarity(id)]);
	}

	function formatPrice(v) {
		// 千位分隔，和游戏里价签的读法一致
		var s = String(Math.round(v));
		var out = "";
		for (var i = 0; i < s.length; ++i) {
			if (i > 0 && ((s.length - i) % 3) === 0) { out += ","; }
			out += s.charAt(i);
		}
		return out;
	}

	var g_Grid = null;
	var g_Details = null;
	var g_Placed = 0;
	var g_Skipped = 0;
	var g_UsedCells = 0;
	var g_Wanted = [];
	var g_Missing = [];
	var g_Items = [];			// 背包里的每件东西 { id, w, h, x, y, panel }
	var g_Drag = null;			// 拖动中的状态（见 startDrag/endDrag）
	var g_Scale = 0;			// 设备像素 / 逻辑像素（这个移植的窗口逻辑空间是 1920x1080，见 deviceScale）
	var g_HoverItem = null;		// 当前光标下的物品（tick 做命中测试，见 backpackTick）
	var g_hDenyInput = 0;		// AddDenyAllInputToGame 的句柄（把鼠标从游戏手里要过来）
	var g_Window = null;		// SePackWindow：弹窗跟随鼠标时的定位基准

	// ------------------------------------------------------------------------
	// 数据小工具
	// ------------------------------------------------------------------------
	function itemDef(id) {
		// "se_store_<defindex>_<paintkit>"（见 se_session_sim.js::GetFauxItemIDFromDefAndPaintIndex）
		var s = String(id === undefined || id === null ? "" : id);
		var parts = s.split("_");
		return parts.length >= 3 ? parts[2] : "";
	}

	function itemSize(id) {
		var d = itemDef(id);
		return ITEM_SIZE.hasOwnProperty(d) ? ITEM_SIZE[d] : [2, 2];
	}

	function itemName(id) {
		var n = "";
		try { n = InventoryAPI.GetItemName(id); } catch (e) { n = ""; }
		if (!n) { return ""; }
		try { n = $.Localize(n); } catch (e2) { /* 已经本地化好的就直接用 */ }
		return String(n === undefined || n === null ? "" : n);
	}

	function itemRarityColor(id) {
		try { return String(InventoryAPI.GetItemRarityColor(id) || "#6b7680"); }
		catch (e) { return "#6b7680"; }
	}

	function itemRarityName(id) {
		// 稀有度数字 -> 中文档位（CS:GO 的 0..6 / 7 是刀"金"档，见 se_faux_econ.cpp）
		var r = 0;
		try { r = Number(InventoryAPI.GetItemAttributeValue(id, "rarity")); } catch (e) { r = 0; }
		if (!r) {
			// sim 没给 rarity 字段时，用颜色反查一个近似档位
			var c = itemRarityColor(id).toLowerCase();
			if (c === "#b0c3d9") { r = 1; }
			else if (c === "#5e98d9") { r = 2; }
			else if (c === "#4b69ff") { r = 3; }
			else if (c === "#8847ff") { r = 4; }
			else if (c === "#d32ce6") { r = 5; }
			else if (c === "#eb4b4b") { r = 6; }
			else if (c === "#e4ae39") { r = 7; }
		}
		if (r >= 7) { return "★ 稀有特殊"; }
		return ["消费级", "工业级", "军规级", "受限", "保密", "隐秘"][r >= 1 ? r - 1 : 0] || "消费级";
	}

	// ------------------------------------------------------------------------
	// 网格
	// ------------------------------------------------------------------------
	function buildBackdrop(grid) {
		for (var y = 0; y < ROWS; ++y) {
			for (var x = 0; x < COLS; ++x) {
				var s = $.CreatePanel("Panel", grid, "SePackSlot" + x + "_" + y);
				s.style.width = (CELL - GAP) + "px";
				s.style.height = (CELL - GAP) + "px";
				s.style.x = (x * CELL) + "px";
				s.style.y = (y * CELL) + "px";
				s.style.backgroundColor = "#0d0f12ff";
				s.style.border = "1px solid #1c2127ff";
				s.hittest = false;			// 背景格只负责好看，不抢鼠标
			}
		}
	}

	function canPlace(occ, x, y, w, h) {
		if ((x + w) > COLS || (y + h) > ROWS) { return false; }
		for (var dy = 0; dy < h; ++dy) {
			for (var dx = 0; dx < w; ++dx) {
				if (occ[(x + dx) + "_" + (y + dy)]) { return false; }
			}
		}
		return true;
	}

	function markPlaced(occ, x, y, w, h) {
		for (var dy = 0; dy < h; ++dy) {
			for (var dx = 0; dx < w; ++dx) {
				occ[(x + dx) + "_" + (y + dy)] = true;
			}
		}
	}

	function makeItem(grid, id, index, x, y, w, h) {
		var cell = $.CreatePanel("Panel", grid, "SePackItem" + index);
		cell.style.x = (x * CELL) + "px";
		cell.style.y = (y * CELL) + "px";
		cell.style.width = ((w * CELL) - GAP) + "px";
		cell.style.height = ((h * CELL) - GAP) + "px";
		cell.style.backgroundColor = "#22282fff";
		cell.style.border = "1px solid " + itemRarityColor(id);
		cell.style.padding = "2px";

		// 图标：ItemImage 是移植过来的 C++ 面板（csgo_item_image_panel.cpp），
		// 只要把 itemid 给它，名字/贴图由 C++ 侧解析（和 itemtile.js 的做法一致）。
		var img = $.CreatePanel("ItemImage", cell, "SePackImage" + index);
		img.style.width = "100%";
		img.style.height = "58%";		// 58/26/16 分配，给右下角的价格留一行
		try { img.itemid = id; } catch (e) { }

		var label = $.CreatePanel("Label", cell, "SePackLabel" + index);
		label.style.width = "100%";
		label.style.height = "26%";
		label.style.fontSize = (w >= 4 ? "15px" : (w >= 2 ? "13px" : "11px"));
		label.style.color = "#c9d1d9ff";
		label.style.textAlign = "center";
		label.style.textOverflow = "ellipsis";
		label.text = itemName(id);

		// 价格：格子右下角（小字，绿色；一眼能看出哪件值钱）
		var price = $.CreatePanel("Label", cell, "SePackPrice" + index);
		price.style.width = "100%";
		price.style.height = "16%";
		price.style.fontSize = (w >= 4 ? "14px" : "12px");
		price.style.color = "#b0e57cff";
		price.style.textAlign = "right";
		price.style.marginRight = "4px";
		price.text = "¥" + formatPrice(itemPrice(id));

		var item = { id: id, w: w, h: h, x: x, y: y, panel: cell };
		g_Items.push(item);

		// 悬停/高亮不靠 onmouseover/onmouseout：这个移植里 hover 事件的到达不稳定（用户实测
		// "悬停没有反应，只有点击才有"），改为由 backpackTick 自己按光标位置做命中测试。
		// onactivate 保留：点击时也刷一次详情（顺带当兜底）。
		cell.SetPanelEvent("onactivate", function () { showDetails(id); });

		// 拖动：按下抓起来（onmousedown），松手落下（onmouseup）。panorama 的 mousedown 会把鼠标
		// 捕获到这个面板，所以 mouseup 也会回到这里 —— 这正是 itemtile 那类面板做不到拖动的原因：
		// 它没有鼠标坐标。坐标由新加的 GetCursorPositionWithinWindow() 提供（见 panel2d.cpp）。
		cell.SetPanelEvent("onmousedown", function () { startDrag(item); });
		cell.SetPanelEvent("onmouseup", function () { endDrag(true); });

		return cell;
	}

	// ------------------------------------------------------------------------
	// 拖动（塔科夫式：物品跟着鼠标走，落点按格子吸附，合法/非法用边框颜色区分）
	//
	// 实现要点：
	//   * ghost 面板：拖动期间新建一个"影子"面板挂在网格下 —— panorama 按子节点顺序绘制，
	//     新节点天然在最上层，所以不需要 z-index（这个版本也没把 z-index 暴露给 JS）。
	//   * 原格子保持可见但压暗（不 collapse）：鼠标捕获在它身上，收起它就收不到 mouseup 了。
	//   * 每帧用 $.Schedule 轮询光标位置换算成格子坐标（panorama 的鼠标事件不带坐标）。
	// ------------------------------------------------------------------------
	function rebuildOccExcept(skip) {
		var occ = {};
		for (var i = 0; i < g_Items.length; ++i) {
			var it = g_Items[i];
			if (it === skip) { continue; }
			markPlaced(occ, it.x, it.y, it.w, it.h);
		}
		return occ;
	}

	function gridLocalCursor() {
		// 光标与网格位置都在窗口（surface）坐标系里，相减即网格本地坐标
		var cur = null;
		try { cur = g_Grid.GetCursorPositionWithinWindow(); } catch (e) { return null; }
		if (!cur) { return null; }

		var gp = null;
		try { gp = g_Grid.GetPositionWithinWindow(); } catch (e2) { gp = null; }
		if (!gp) { return null; }

		// 这里相减得到的是**设备像素**（1280x720 那份），而格子尺寸 CELL 是布局用的**逻辑像素**
		// （panorama 的 1920x1080 空间，本移植的窗口缩放是 height/1080 = 0.667）。不换算的话
		// 物品只跟到 2/3 的距离，小幅拖动会原地弹回 —— 看起来就是"拖不动"。
		var s = deviceScale();
		return { x: (Number(cur.x) - Number(gp.x)) / s, y: (Number(cur.y) - Number(gp.y)) / s };
	}

	// 标定"设备像素 / 逻辑像素"：网格背景格是自己按 CELL 摆的，位置已知（第 0 格 x=0，第 2 格
	// x=2*CELL），拿它们的窗口坐标差一除就得到比例。这样不依赖引擎暴露缩放系数。
	function deviceScale() {
		if (g_Scale > 0) { return g_Scale; }

		// 注意：onLoad 那一刻布局还没跑，GetPositionWithinWindow() 全是 0，标定会失败。
		// 失败时**不能**把 1 缓存下来，否则之后永远按 1 用（第一版就这么错的，拖动只有 2/3 距离）。
		var s = 0;
		try {
			var a = g_Grid.FindChildTraverse("SePackSlot0_0");
			var b = g_Grid.FindChildTraverse("SePackSlot2_0");
			if (a && b) {
				var pa = a.GetPositionWithinWindow();
				var pb = b.GetPositionWithinWindow();
				var d = Number(pb.x) - Number(pa.x);
				if (d > 1) { s = d / (2 * CELL); }
			}
		} catch (e) { }

		if (s > 0) {
			g_Scale = s;
			$.Msg("[SE port] 背包: 坐标标定 scale=" + g_Scale);
		}

		return g_Scale > 0 ? g_Scale : 1;
	}

	function makeGhost(it) {
		var g = $.CreatePanel("Panel", g_Grid, "SePackGhost");
		g.style.width = ((it.w * CELL) - GAP) + "px";
		g.style.height = ((it.h * CELL) - GAP) + "px";
		g.style.x = (it.x * CELL) + "px";
		g.style.y = (it.y * CELL) + "px";
		g.style.backgroundColor = "#2b333cff";
		g.style.border = "2px solid " + itemRarityColor(it.id);
		g.style.opacity = "0.9";
		g.hittest = false;			// 别抢鼠标，否则 mouseup 收不到
		g.hittestchildren = false;	// 里面的图/字也不能抢（否则悬停命中会被它挡住）

		var img = $.CreatePanel("ItemImage", g, "SePackGhostImage");
		img.style.width = "100%";
		img.style.height = "70%";
		try { img.itemid = it.id; } catch (e) { }

		var label = $.CreatePanel("Label", g, "SePackGhostLabel");
		label.style.width = "100%";
		label.style.height = "30%";
		label.style.fontSize = "12px";
		label.style.color = "#e6ebf0ff";
		label.style.textAlign = "center";
		label.style.textOverflow = "ellipsis";
		label.text = itemName(it.id);

		return g;
	}

	function startDrag(it) {
		if (g_Drag || !g_Grid) { return; }

		var local = gridLocalCursor();
		if (!local) {
			$.Msg("[SE port] 背包: 拖动起手失败 —— 拿不到光标坐标（GetCursorPositionWithinWindow）");
			return;
		}

		g_Drag = {
			item: it,
			offX: local.x - (it.x * CELL),	// 抓取点相对物品左上角的偏移：拖动时不跳
			offY: local.y - (it.y * CELL),
			origX: it.x,
			origY: it.y,
			targetX: it.x,
			targetY: it.y,
			valid: true,
			ghost: makeGhost(it)
		};

		it.panel.style.opacity = "0.3";
		hideDetails();			// 拖动时收起详情（塔科夫也是这样）
	}

	// 光标命中测试：返回光标（网格本地、逻辑像素）落在哪件物品上
	function hitTestItem(local, skip) {
		if (!local) { return null; }
		for (var i = 0; i < g_Items.length; ++i) {
			var it = g_Items[i];
			if (it === skip) { continue; }
			var x0 = it.x * CELL, y0 = it.y * CELL;
			if (local.x >= x0 && local.x < x0 + it.w * CELL && local.y >= y0 && local.y < y0 + it.h * CELL) {
				return it;
			}
		}
		return null;
	}

	// 悬停详情 + 拖动，都由这一条 tick 驱动（每帧读一次光标位置）。
	function backpackTick() {
		try {
			var local = gridLocalCursor();

			if (g_Drag) {
				updateDrag(local);
			} else {
				var hit = hitTestItem(local, null);
				if (hit) {
					if (g_HoverItem !== hit) {
						g_HoverItem = hit;
						showDetails(hit.id);
					}
				} else if (g_HoverItem) {
					g_HoverItem = null;
					hideDetails();
				}
				// 详情浮层跟着鼠标走（CS:GO/塔科夫那种跟随式 tooltip）
				if (g_HoverItem) { placeDetailsAtCursor(); }
			}
		} catch (e) {
			$.Msg("[SE port] 背包: backpackTick 异常: " + e);
		}

		$.Schedule(0.033, backpackTick);
	}

	function updateDrag(local) {
		var d = g_Drag;
		if (!local) { return; }

		var it = d.item;
		var x = Math.round((local.x - d.offX) / CELL);
		var y = Math.round((local.y - d.offY) / CELL);
		x = Math.max(0, Math.min(COLS - it.w, x));
		y = Math.max(0, Math.min(ROWS - it.h, y));

		d.targetX = x;
		d.targetY = y;
		d.valid = canPlace(rebuildOccExcept(it), x, y, it.w, it.h);

		if (d.ghost) {
			d.ghost.style.x = (x * CELL) + "px";
			d.ghost.style.y = (y * CELL) + "px";
			d.ghost.style.border = "2px solid " + (d.valid ? "#4cd964ff" : "#ff3b30ff");
		}
	}

	function endDrag(commit) {
		if (!g_Drag) { return; }

		var d = g_Drag;
		g_Drag = null;

		var it = d.item;
		if (d.ghost) { d.ghost.DeleteAsync(0.0); }
		it.panel.style.opacity = "1";

		var moved = commit && d.valid && (d.targetX !== d.origX || d.targetY !== d.origY);
		if (moved) {
			it.x = d.targetX;
			it.y = d.targetY;
			it.panel.style.x = (it.x * CELL) + "px";
			it.panel.style.y = (it.y * CELL) + "px";
		}

		$.Msg("[SE port] 背包: 拖动 " + itemName(it.id) + " (" + d.origX + "," + d.origY + ") -> ("
			+ d.targetX + "," + d.targetY + ") " + (moved ? "已放下" : (commit ? "回原位" : "取消")));
	}

	function fillGrid(grid) {
		var occ = {};
		g_Placed = 0;
		g_Skipped = 0;
		g_UsedCells = 0;
		g_Wanted = [];
		g_Missing = [];
		g_Items = [];
		g_Drag = null;

		// 1) 把库存按 defindex 索引一遍（1087 条只走一次，很快）：每个 def 优先取"无皮肤"那条
		var firstId = {}, baseId = {};
		var n = 0;
		try { n = Number(InventoryAPI.GetInventoryCount()) || 0; } catch (e) { n = 0; }

		for (var i = 0; i < n; ++i) {
			var id = "";
			try { id = InventoryAPI.GetInventoryItemIDByIndex(i); } catch (e2) { id = ""; }
			if (!id) { continue; }

			var d = itemDef(id);
			if (!firstId.hasOwnProperty(d)) { firstId[d] = id; }
			if (itemPaint(id) === "0" && !baseId.hasOwnProperty(d)) { baseId[d] = id; }
		}

		// 2) 只留下清单里的东西，按占格从大到小排（摆起来更紧凑、也像塔科夫）
		var list = [];
		for (var k = 0; k < WANTED_DEFS.length; ++k) {
			var dd = String(WANTED_DEFS[k]);
			var wid = baseId.hasOwnProperty(dd) ? baseId[dd] : (firstId.hasOwnProperty(dd) ? firstId[dd] : "");
			if (!wid) { g_Missing.push(dd); continue; }

			var sz = itemSize(wid);
			list.push({ id: wid, w: sz[0], h: sz[1] });
		}
		list.sort(function (a, b) { return (b.w * b.h) - (a.w * a.h); });

		// 3) 逐个找位置放
		for (var j = 0; j < list.length; ++j) {
			var w = list[j].w, h = list[j].h, ok = false;
			for (var y = 0; y < ROWS && !ok; ++y) {
				for (var x = 0; x < COLS && !ok; ++x) {
					if (canPlace(occ, x, y, w, h)) {
						markPlaced(occ, x, y, w, h);
						makeItem(grid, list[j].id, j, x, y, w, h);
						g_Placed++;
						g_UsedCells += w * h;
						ok = true;
					}
				}
			}
			if (!ok) { g_Skipped++; }
		}

		$.Msg("[SE port] 背包: 清单 " + WANTED_DEFS.length + " 件 -> 放入 " + g_Placed + " 件, 占 "
			+ g_UsedCells + "/" + (COLS * ROWS) + " 格, 放不下 " + g_Skipped
			+ (g_Missing.length ? (", 目录里没有的 def: " + g_Missing.join(",")) : ""));
	}

	// ------------------------------------------------------------------------
	// 详情
	// ------------------------------------------------------------------------
	function showDetails(id) {
		if (!g_Details) { return; }

		var elName = g_Details.FindChildTraverse("SePackDetailName");
		var elRar = g_Details.FindChildTraverse("SePackDetailRarity");
		var elMeta = g_Details.FindChildTraverse("SePackDetailMeta");
		var elImg = g_Details.FindChildTraverse("SePackDetailImage");
		var elPrice = g_Details.FindChildTraverse("SePackDetailPrice");

		if (elName) { elName.text = itemName(id); }
		if (elRar) {
			elRar.text = itemRarityName(id);
			elRar.style.color = itemRarityColor(id);
		}
		if (elPrice) {
			// 价格：本构建没有真价目表，见 itemPrice() 的注释
			elPrice.text = "¥" + formatPrice(itemPrice(id));
		}
		if (elMeta) {
			var size = itemSize(id);
			elMeta.text = "占用 " + size[0] + " x " + size[1] + " 格\n定义索引 " + itemDef(id) + "\n" + itemDef2Class(id);
		}
		if (elImg) {
			try { elImg.itemid = id; } catch (e) { }
		}
		g_Details.style.visibility = "visible";
	}

	function hideDetails() {
		if (g_Details) { g_Details.style.visibility = "collapse"; }
	}

	// 把详情浮层放到光标旁边：右/下放不下就翻到另一侧，再整体夹在窗口内。
	// 光标位置和窗口位置都在"设备像素"里（GetCursorPositionWithinWindow /
	// GetPositionWithinWindow 同一坐标系），除以前面标定出的 scale 才是布局用的逻辑像素。
	function placeDetailsAtCursor() {
		if (!g_Details || !g_Window) { return; }

		var cur = null;
		try { cur = g_Window.GetCursorPositionWithinWindow(); } catch (e) { return; }
		if (!cur) { return; }

		var wp = null;
		try { wp = g_Window.GetPositionWithinWindow(); } catch (e2) { return; }
		if (!wp) { return; }

		var s = deviceScale();
		var cx = (Number(cur.x) - Number(wp.x)) / s;		// 光标在窗口内的逻辑坐标
		var cy = (Number(cur.y) - Number(wp.y)) / s;

		// actuallayoutwidth/height 是**设备像素**（和光标位置同一个空间），而 x/y 要写逻辑像素 ——
		// 忘了这层换算就会拿"设备宽的窗口"去夹"逻辑坐标的弹窗"：右侧物品翻转后会被夹到 x≈211，
		// 看起来就是"弹窗跑到中间去"（用户报的现象）。
		var flWinW = (Number(g_Window.actuallayoutwidth) / s) || 928;
		var flWinH = (Number(g_Window.actuallayoutheight) / s) || 670;
		var flTipW = (Number(g_Details.actuallayoutwidth) / s) || 300;
		var flTipH = (Number(g_Details.actuallayoutheight) / s) || 210;
		var flGap = 18;

		var x = cx + flGap;
		if (x + flTipW > flWinW - 4) { x = cx - flGap - flTipW; }		// 右边放不下 → 翻到左侧
		if (x < 4) { x = 4; }
		if (x + flTipW > flWinW - 4) { x = Math.max(4, flWinW - 4 - flTipW); }

		var y = cy + flGap;
		if (y + flTipH > flWinH - 4) { y = cy - flGap - flTipH; }		// 下面放不下 → 翻到上方
		if (y < 4) { y = 4; }
		if (y + flTipH > flWinH - 4) { y = Math.max(4, flWinH - 4 - flTipH); }

		g_Details.style.x = Math.round(x) + "px";
		g_Details.style.y = Math.round(y) + "px";
	}

	function itemDef2Class(id) {
		// 只用于详情里凑一行"类型"信息：从 sub/def 段推不出来，就直接显示 ID
		return String(id);
	}

	// ------------------------------------------------------------------------
	// 初始化
	// ------------------------------------------------------------------------
	function onLoad() {
		var root = $.GetContextPanel();
		if (!root) { return; }

		g_Grid = root.FindChildTraverse("SePackGrid");
		g_Details = root.FindChildTraverse("SePackDetails");
		g_Window = root.FindChildTraverse("SePackWindow");

		if (g_Grid) {
			buildBackdrop(g_Grid);
			fillGrid(g_Grid);
		}

		var sub = root.FindChildTraverse("SePackSubtitle");
		if (sub) { sub.text = g_Placed + " 件装备"; }

		var foot = root.FindChildTraverse("SePackFooterText");
		if (foot) {
			foot.text = "占用 " + g_UsedCells + " / " + (COLS * ROWS) + " 格 · " + g_Placed + " 件装备"
				+ (g_Skipped > 0 ? (" · " + g_Skipped + " 件放不下") : "")
				+ (g_Missing.length ? (" · 目录缺 def " + g_Missing.join(",")) : "");
		}

		var close = root.FindChildTraverse("SePackClose");
		if (close) {
			close.SetPanelEvent("onactivate", function () {
				// 关掉自己：走引擎命令，让引擎把背包视图整体销毁（和打开对称）
				try { GameInterfaceAPI.ConsoleCommand("se_backpack 0"); } catch (e) { }
				// 最外层面板没有 id（panorama 规定 <root> 的第一层不能带 id，带了整个布局加载失败），
				// 所以这里直接收 context panel 自己。
				// 同时把鼠标还给游戏（和打开时的 AddDenyAllInputToGame 对称）
				if (g_hDenyInput) {
					try { UiToolkitAPI.ReleaseDenyAllInputToGame(g_hDenyInput); } catch (e) { }
					g_hDenyInput = 0;
				}
				var r = $.GetContextPanel();
				if (r) { r.style.visibility = "collapse"; }
			});
		}

		// 松手兜底：鼠标在物品外面松开时，onmouseup 可能到不了那个物品格；网格和整窗也各挂一份，
		// 保证拖动一定会结束（否则 ghost 会留在画面上）。
		if (g_Grid) { g_Grid.SetPanelEvent("onmouseup", function () { endDrag(true); }); }
		root.SetPanelEvent("onmouseup", function () { endDrag(true); });

		// 注意：**不要**在这里调 UiToolkitAPI.AddDenyAllInputToGame()。那是 CS:GO 主菜单接管鼠标的做法
		// （csgo_mainmenu.cpp:319），但这个移植里请求之后引擎会把 cl_mouseenable 置 0，而客户端的
		// cl_mouseenable_buttons（"鼠标关掉也保留按键"）**只声明、没人读**（game/client/in_mouse.cpp:117），
		// 结果按下事件也一起没了（实测 SE_PORT_MOUSEDOWN 计数 0，点击/拖动全废）。鼠标位置改由
		// GetCursorPositionWithinWindow() 直接读实时系统光标，不需要跟游戏抢鼠标。

		// 悬停 + 拖动都由这条 tick 驱动（不依赖 hover 事件是否到达）
		backpackTick();
	}

	$.Schedule(0.0, onLoad);
})();
