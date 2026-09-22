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
	var COLS = 12;
	var ROWS = 8;
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

	var g_Grid = null;
	var g_Details = null;
	var g_Placed = 0;
	var g_Skipped = 0;

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
		img.style.height = "70%";
		try { img.itemid = id; } catch (e) { }

		var label = $.CreatePanel("Label", cell, "SePackLabel" + index);
		label.style.width = "100%";
		label.style.height = "30%";
		label.style.fontSize = (w >= 4 ? "15px" : (w >= 2 ? "13px" : "11px"));
		label.style.color = "#c9d1d9ff";
		label.style.textAlign = "center";
		label.style.textOverflow = "ellipsis";
		label.text = itemName(id);

		cell.SetPanelEvent("onmouseover", function () { showDetails(id); });
		cell.SetPanelEvent("onmouseout", function () { hideDetails(); });
		cell.SetPanelEvent("onactivate", function () { showDetails(id); });

		return cell;
	}

	function fillGrid(grid) {
		var occ = {};
		g_Placed = 0;
		g_Skipped = 0;

		// 真物品目录：sim 侧按排序/搜索返回结果集（这里要全部 -> 空搜索 + 字母序）
		try { InventoryAPI.SetInventorySortAndFilters("inv_sort_alpha", true, ""); } catch (e) { }

		var n = 0;
		try { n = Number(InventoryAPI.GetInventoryCount()) || 0; } catch (e2) { n = 0; }

		for (var i = 0; i < n; ++i) {
			var id = "";
			try { id = InventoryAPI.GetInventoryItemIDByIndex(i); } catch (e3) { id = ""; }
			if (!id) { continue; }

			var size = itemSize(id);
			var w = size[0], h = size[1];

			var placed = false;
			for (var y = 0; y < ROWS && !placed; ++y) {
				for (var x = 0; x < COLS && !placed; ++x) {
					if (canPlace(occ, x, y, w, h)) {
						markPlaced(occ, x, y, w, h);
						makeItem(grid, id, i, x, y, w, h);
						g_Placed++;
						placed = true;
					}
				}
			}
			if (!placed) { g_Skipped++; }
		}
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

		if (elName) { elName.text = itemName(id); }
		if (elRar) {
			elRar.text = itemRarityName(id);
			elRar.style.color = itemRarityColor(id);
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

		if (g_Grid) {
			buildBackdrop(g_Grid);
			fillGrid(g_Grid);
		}

		var sub = root.FindChildTraverse("SePackSubtitle");
		if (sub) { sub.text = g_Placed + " 件物品在背包里"; }

		var foot = root.FindChildTraverse("SePackFooterText");
		if (foot) {
			foot.text = "共 " + (g_Placed + g_Skipped) + " 件 · 背包容量 " + (COLS * ROWS) + " 格 · 放入 " + g_Placed + " 件"
				+ (g_Skipped > 0 ? (" · 放不下 " + g_Skipped + " 件") : "");
		}

		var close = root.FindChildTraverse("SePackClose");
		if (close) {
			close.SetPanelEvent("onactivate", function () {
				// 关掉自己：走引擎命令，让引擎把背包视图整体销毁（和打开对称）
				try { GameInterfaceAPI.ConsoleCommand("se_backpack 0"); } catch (e) { }
				var r = $.GetContextPanel().FindChildTraverse("SePackRoot");
				if (r) { r.style.visibility = "collapse"; }
			});
		}

		$.Msg("[SE port] 背包: 放入 " + g_Placed + " 件, 放不下 " + g_Skipped + " 件 (网格 " + COLS + "x" + ROWS + ")");
	}

	$.Schedule(0.0, onLoad);
})();
