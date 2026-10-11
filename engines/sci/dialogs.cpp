/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "engines/advancedDetector.h"
#include "base/plugins.h"
#include "common/file.h"
#include "common/hashmap.h"
#include "common/ptr.h"
#include "common/translation.h"

#include "sci/detection.h"
#include "sci/dialogs.h"
#include "sci/sci.h"
#include "sci/engine/guest_additions.h"

#include "sci/detection_options.h"

namespace Sci {

enum {
	kPhant2VolumeCapChanged = 'p2vc'
};

OptionsWidget::OptionsWidget(GuiObject *boss, const Common::String &name, const Common::String &domain) :
		OptionsContainerWidget(boss, name, "SciGameOptionsDialog", domain) {
	_guiOptions = ConfMan.get("guioptions", domain);

	_isPhant2 = (ConfMan.get("gameid", domain) == "phantasmagoria2");
	_phant2VolumeSlider = nullptr;
	_phant2VolumeValue = nullptr;
	if (_isPhant2) {
		GUI::StaticTextWidget *capLabel = new GUI::StaticTextWidget(widgetsBoss(),
			_dialogLayout + ".phant2_volume_cap_desc",
			_("Music/SFX vs. movie dialogue:"),
			_("How far the in-game music and sound effects sit below movie dialogue. "
			  "Phantasmagoria 2 plays speech inside its movies, so lowering this is the "
			  "only way to stop music burying it. Around -20 dB is a normal mix."));
		capLabel->setAlign(Graphics::kTextAlignRight);

		_phant2VolumeSlider = new GUI::SliderWidget(widgetsBoss(),
			_dialogLayout + ".phant2_volume_cap", Common::U32String(), kPhant2VolumeCapChanged);
		_phant2VolumeSlider->setMinValue(kPhant2VolumeCapMin);
		_phant2VolumeSlider->setMaxValue(kPhant2VolumeCapMax);

		_phant2VolumeValue = new GUI::StaticTextWidget(widgetsBoss(),
			_dialogLayout + ".phant2_volume_cap_value", Common::U32String());
		_phant2VolumeValue->setFlags(GUI::WIDGET_CLEARBG);
	}

	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_checkboxes[entry->option.configOption] = new GUI::CheckboxWidget(widgetsBoss(), _dialogLayout + "." + entry->option.configOption, _(entry->option.label), _(entry->option.tooltip));

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions)) {
			GUI::StaticTextWidget *textWidget = new GUI::StaticTextWidget(widgetsBoss(), _dialogLayout + "." + entry->configOption + "_desc", _(entry->label), _(entry->tooltip));
			textWidget->setAlign(Graphics::kTextAlignRight);

			_popUps[entry->configOption] = new GUI::PopUpWidget(widgetsBoss(), _dialogLayout + "." + entry->configOption);

			for (uint i = 0; entry->items[i].label; ++i)
				_popUps[entry->configOption]->appendEntry(_(entry->items[i].label), entry->items[i].configValue);
		}
}

void OptionsWidget::defineLayout(GUI::ThemeEval &layouts, const Common::String &layoutName, const Common::String &overlayedLayout) const {
	layouts.addDialog(layoutName, overlayedLayout);
	layouts.addLayout(GUI::ThemeLayout::kLayoutVertical).addPadding(0, 0, 0, 0);

	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		layouts.addWidget(entry->option.configOption, "Checkbox");

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry) {
		layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0);
		layouts.addWidget(Common::String(entry->configOption) + "_desc", "OptionsLabel");
		layouts.addWidget(entry->configOption, "PopUp").closeLayout();
	}

	layouts.addLayout(GUI::ThemeLayout::kLayoutHorizontal).addPadding(0, 0, 0, 0);
	layouts.addWidget("phant2_volume_cap_desc", "OptionsLabel");
	layouts.addWidget("phant2_volume_cap", "Slider");
	layouts.addWidget("phant2_volume_cap_value", "ShortOptionsLabel").closeLayout();

	layouts.closeLayout().closeDialog();
}

void OptionsWidget::load() {
	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_checkboxes[entry->option.configOption]->setState(ConfMan.getBool(entry->option.configOption, _domain));

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			_popUps[entry->configOption]->setSelectedTag(ConfMan.getInt(entry->configOption, _domain));

	if (_phant2VolumeSlider != nullptr) {
		int cap = kPhant2VolumeMax;
		if (ConfMan.hasKey(PHANT2_VOLUME_CAP_CONFIG_KEY, _domain)) {
			cap = ConfMan.getInt(PHANT2_VOLUME_CAP_CONFIG_KEY, _domain);
		}
		if (cap < kPhant2VolumeCapMin) {
			cap = kPhant2VolumeCapMin;
		}
		if (cap > kPhant2VolumeCapMax) {
			cap = kPhant2VolumeCapMax;
		}
		_phant2VolumeSlider->setValue(cap);
		updatePhant2VolumeLabel();
	}

	// If the deprecated native_fb01 option is set, use it to set midi_mode
	if (ConfMan.hasKey("native_fb01", _domain) && ConfMan.getBool("native_fb01", _domain))
		_popUps["midi_mode"]->setSelectedTag(kMidiModeFB01);
}

bool OptionsWidget::save() {
	for (const ADExtraGuiOptionsMap *entry = optionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			ConfMan.setBool(entry->option.configOption, _checkboxes[entry->option.configOption]->getState(), _domain);

	for (const PopUpOptionsMap *entry = popUpOptionsList; entry->guioFlag; ++entry)
		if (checkGameGUIOption(entry->guioFlag, _guiOptions))
			ConfMan.setInt(entry->configOption, _popUps[entry->configOption]->getSelectedTag(), _domain);

	if (_phant2VolumeSlider != nullptr) {
		ConfMan.setInt(PHANT2_VOLUME_CAP_CONFIG_KEY, _phant2VolumeSlider->getValue(), _domain);
	}

	// Remove deprecated option
	ConfMan.removeKey("native_fb01", _domain);

	return true;
}

void OptionsWidget::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kPhant2VolumeCapChanged:
		updatePhant2VolumeLabel();
		break;
	default:
		GUI::OptionsContainerWidget::handleCommand(sender, cmd, data);
		break;
	}
}

void OptionsWidget::updatePhant2VolumeLabel() {
	if (_phant2VolumeValue == nullptr || _phant2VolumeSlider == nullptr) {
		return;
	}
	const int cap = _phant2VolumeSlider->getValue();
	// Volume is a linear amplitude multiplier, so report dB: that is the number
	// that predicts what the user will actually hear.
	const double decibels = 20.0 * log10((double)cap / (double)kPhant2VolumeCapMax);
	_phant2VolumeValue->setLabel(Common::String::format("%.1f dB", decibels));
}

} // End of namespace Sci
