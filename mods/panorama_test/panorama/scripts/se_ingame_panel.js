"use strict";
// SE port (2026-09-22): 地图内面板测试的 JS。
// 只做两件事：证明"地图内 panorama 的 JS 在跑"，并每秒刷新一行信息（截图里能看出是活的）。
(function () {
	var g_nTick = 0;
	var g_flStart = Date.now();

	function pad2(n) { return (n < 10 ? "0" : "") + n; }

	function tick() {
		var root = $.GetContextPanel();
		if (!root) { return; }

		g_nTick++;

		var el = root.FindChildTraverse("SeIgInfo");
		if (el) {
			var d = new Date();
			// 中英混排：万一 CJK 字形还不行，也能从英文那半判断是"整块不渲染"还是"缺中文字形"
			el.text = "tick #" + g_nTick
				+ "   心跳 #" + g_nTick
				+ "   run " + ((Date.now() - g_flStart) / 1000).toFixed(1) + "s"
				+ "   clock " + pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds())
				+ "   时钟 " + pad2(d.getHours()) + ":" + pad2(d.getMinutes());
		}

		var el2 = root.FindChildTraverse("SeIgInfo2");
		if (el2) {
			// 面板尺寸（panorama 的 JS 属性；拿不到就退化成空字符串，不影响测试）
			var w = "", h = "";
			try { w = String(root.actuallayoutwidth); h = String(root.actuallayoutheight); } catch (e) { w = "?"; h = "?"; }
			el2.text = "root size " + w + " x " + h + "   根面板尺寸 " + w + " x " + h;
		}

		$.Schedule(1.0, tick);
	}

	$.Schedule(0.5, tick);
	$.Msg("[SE port] 地图内面板测试已启动（se_ingame_panel.js）");
})();
