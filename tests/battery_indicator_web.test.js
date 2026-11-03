const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const source = fs.readFileSync('main\\web\\script.js', 'utf8');
const context = {};
vm.runInNewContext(source.slice(0, source.indexOf('class LanguageManager')), context);
const config = () => ({
    enabled: true, key: 'left',
    rules: [
        { min_percent: 76, max_percent: 100, color: '#00ff00', interval_seconds: 30 },
        { min_percent: 30, max_percent: 75, color: '#ffff00', interval_seconds: 60 },
        { min_percent: 0, max_percent: 19, color: '#ff0000', interval_seconds: 10 }
    ]
});

test('battery configuration validator exists', () => {
    assert.equal(typeof context.validateBatteryIndicatorConfig, 'function');
});
test('inclusive ranges and deliberate gaps are valid', () => {
    assert.equal(context.validateBatteryIndicatorConfig(config()), null);
});
test('overlapping endpoints are rejected', () => {
    const value = config();
    value.rules[1].max_percent = 76;
    assert.equal(context.validateBatteryIndicatorConfig(value), 'batteryIndicatorOverlap');
});
test('percentage, interval and color are strictly validated', () => {
    for (const [field, invalid] of [
        ['min_percent', -1], ['min_percent', 100.5], ['max_percent', 101],
        ['max_percent', '100'], ['interval_seconds', 0], ['interval_seconds', 86401],
        ['interval_seconds', 1.5], ['interval_seconds', NaN], ['color', '#fff'],
        ['color', '#gg0000']
    ]) {
        const value = config();
        value.rules[0][field] = invalid;
        assert.equal(context.validateBatteryIndicatorConfig(value), 'batteryIndicatorInvalidRule');
    }
});
test('one selected key, boolean enable and bounded rules are required', () => {
    for (const value of [
        { ...config(), key: 'both' }, { ...config(), enabled: 1 },
        { ...config(), rules: [] }, { ...config(), rules: Array(9).fill(config().rules[0]) },
        { ...config(), rules: null }, null
    ]) {
        assert.equal(context.validateBatteryIndicatorConfig(value), 'batteryIndicatorInvalidConfig');
    }
    assert.equal(context.validateBatteryIndicatorConfig({ enabled: false, key: 'right', rules: [] }), null);
});

function controllerFixture() {
    const timers = new Map();
    let timerId = 0;
    const elements = {
        batteryIndicatorFields: { disabled: false },
        batteryIndicatorAdd: { disabled: false },
        batteryIndicatorRules: { children: [] }
    };
    const ctx = {
        document: { getElementById: id => elements[id] },
        languageManager: { getText: key => key },
        WebSocket: { OPEN: 1 },
        clearTimeout(id) { timers.delete(id); },
        setTimeout(callback) { timers.set(++timerId, callback); return timerId; },
        validateBatteryIndicatorConfig: context.validateBatteryIndicatorConfig
    };
    vm.runInNewContext(source.slice(source.indexOf('class DualKeyController'),
        source.indexOf('// 页面加载完成后初始化')) + '\nglobalThis.Controller = DualKeyController;', ctx);
    const controller = Object.create(ctx.Controller.prototype);
    Object.assign(controller, {
        batteryIndicatorLoaded: true, batteryIndicatorDirty: true, batteryIndicatorSaving: false,
        batteryIndicatorSequence: 0, batteryIndicatorRequest: null, batteryIndicatorTimer: null,
        websocket: { readyState: 1 }, sent: [], rendered: [], feedback: []
    });
    controller.sendMessage = message => controller.sent.push(message);
    controller.readBatteryIndicatorDraft = config;
    controller.renderBatteryIndicatorRules = value => controller.rendered.push(value);
    controller.setBatteryIndicatorFeedback = key => controller.feedback.push(key);
    controller.expireRequest = () => timers.get(controller.batteryIndicatorTimer)();
    return controller;
}

test('saving sends the firmware command and waits for acknowledgement', () => {
    const controller = controllerFixture();
    controller.saveBatteryIndicatorConfig();
    assert.equal(controller.sent[0].type, 'set_battery_indicator_config');
    assert.deepEqual(controller.sent[0].config, config());
    assert.equal(controller.batteryIndicatorDirty, true);
    assert.equal(controller.batteryIndicatorSaving, true);
    controller.handleBatteryIndicatorConfig({
        type: 'battery_indicator_config', action: 'save', success: true,
        request_id: controller.sent[0].request_id, config: config()
    });
    assert.equal(controller.batteryIndicatorDirty, false);
    assert.equal(controller.batteryIndicatorSaving, false);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorSaved');
});

test('failed save and disconnection preserve unsaved edits', () => {
    const controller = controllerFixture();
    controller.saveBatteryIndicatorConfig();
    controller.handleBatteryIndicatorConfig({
        action: 'save', success: false, error: 'batteryIndicatorSaveFailed',
        request_id: controller.sent[0].request_id, config: config()
    });
    assert.equal(controller.batteryIndicatorDirty, true);
    assert.equal(controller.rendered.length, 0);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorSaveFailed');
    controller.websocket.readyState = 3;
    controller.saveBatteryIndicatorConfig();
    assert.equal(controller.sent.length, 1);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorOffline');
});

test('reconnect loading does not replace unsaved edits; stale replies are ignored', () => {
    const controller = controllerFixture();
    controller.requestBatteryIndicatorConfig();
    controller.handleBatteryIndicatorConfig({
        action: 'load', success: true, request_id: 'stale', config: config()
    });
    assert.equal(controller.rendered.length, 0);
    controller.handleBatteryIndicatorConfig({
        action: 'load', success: true, request_id: controller.sent[0].request_id, config: config()
    });
    assert.equal(controller.rendered.length, 0);
    assert.equal(controller.batteryIndicatorDirty, true);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorUnsaved');
});

test('invalid edits are never sent to the device', () => {
    const controller = controllerFixture();
    controller.readBatteryIndicatorDraft = () => ({ ...config(), rules: [] });
    controller.saveBatteryIndicatorConfig();
    assert.equal(controller.sent.length, 0);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorInvalidConfig');
});

test('timeout preserves edits and ignores a late acknowledgement', () => {
    const controller = controllerFixture();
    controller.saveBatteryIndicatorConfig();
    const requestId = controller.sent[0].request_id;
    controller.expireRequest();
    assert.equal(controller.batteryIndicatorSaving, false);
    assert.equal(controller.batteryIndicatorDirty, true);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorTimeout');
    controller.handleBatteryIndicatorConfig({
        action: 'save', success: true, request_id: requestId, config: config()
    });
    assert.equal(controller.batteryIndicatorDirty, true);
    assert.equal(controller.rendered.length, 0);
});

test('clean load renders canonical configuration and surfaces storage errors', () => {
    const controller = controllerFixture();
    controller.batteryIndicatorDirty = false;
    controller.batteryIndicatorLoaded = false;
    controller.requestBatteryIndicatorConfig();
    const defaults = { ...config(), enabled: false };
    controller.handleBatteryIndicatorConfig({
        action: 'load', success: false, error: 'batteryIndicatorLoadFailed',
        request_id: controller.sent[0].request_id, config: defaults
    });
    assert.equal(controller.batteryIndicatorLoaded, true);
    assert.deepEqual(controller.rendered[0], defaults);
    assert.equal(controller.feedback.at(-1), 'batteryIndicatorLoadFailed');
});
