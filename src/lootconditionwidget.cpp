#include "lootconditionwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// Keep custom translation-wrapper strings visible to lupdate.
static const char *const lootEditorTranslationKeys[] = {
    QT_TRANSLATE_NOOP("LootEditor", "No maximum"),
    QT_TRANSLATE_NOOP("LootEditor", "No enchantment filter"),
    QT_TRANSLATE_NOOP("LootEditor", "Any enchantment (enchanted items only)"),
    QT_TRANSLATE_NOOP("LootEditor", "Enchanted %1"),
    QT_TRANSLATE_NOOP("LootEditor", "Remove"),
    QT_TRANSLATE_NOOP("LootEditor", "Count"),
    QT_TRANSLATE_NOOP("LootEditor", "Enchantment"),
    QT_TRANSLATE_NOOP("LootEditor", "Level"),
    QT_TRANSLATE_NOOP("LootEditor", "Chest content conditions"),
    QT_TRANSLATE_NOOP("LootEditor", "Filter by chest contents"),
    QT_TRANSLATE_NOOP("LootEditor", "Match all (AND)"),
    QT_TRANSLATE_NOOP("LootEditor", "Match any (OR)"),
    QT_TRANSLATE_NOOP("LootEditor", "Item conditions"),
    QT_TRANSLATE_NOOP("LootEditor", "Any structure matches"),
    QT_TRANSLATE_NOOP("LootEditor", "Every structure matches"),
    QT_TRANSLATE_NOOP("LootEditor", "Total across structures in range"),
    QT_TRANSLATE_NOOP("LootEditor", "Multiple structures"),
    QT_TRANSLATE_NOOP("LootEditor", "Chests within structure"),
    QT_TRANSLATE_NOOP("LootEditor", "Bastion Loot version"),
    QT_TRANSLATE_NOOP("LootEditor", "Java 1.16–1.16.1 (original Loot)"),
    QT_TRANSLATE_NOOP("LootEditor", "Java 1.16.2–1.16.5 (tweaked Loot)"),
    QT_TRANSLATE_NOOP("LootEditor",
        "Bastion chest tables changed in Java 1.16.2. The tweaked profile "
        "includes enchanted diamond pickaxes in Hoglin Stable and Generic "
        "chests. The four chest-table categories remain separate."),
    QT_TRANSLATE_NOOP("LootEditor", "Chest coordinates"),
    QT_TRANSLATE_NOOP("LootEditor", "+ Add item condition"),
    QT_TRANSLATE_NOOP("LootEditor", "Total across all chests"),
    QT_TRANSLATE_NOOP("LootEditor", "Any single chest matches"),
    QT_TRANSLATE_NOOP("LootEditor", "Every chest matches"),
    QT_TRANSLATE_NOOP("LootEditor", "Generation-order chest %1"),
    QT_TRANSLATE_NOOP("LootEditor", "Supply chest"),
    QT_TRANSLATE_NOOP("LootEditor", "Map chest"),
    QT_TRANSLATE_NOOP("LootEditor", "Treasure chest"),
    QT_TRANSLATE_NOOP("LootEditor", "Only chest"),
    QT_TRANSLATE_NOOP("LootEditor", "Any position"),
    QT_TRANSLATE_NOOP("LootEditor", "Absolute world coordinates"),
    QT_TRANSLATE_NOOP("LootEditor", "Relative to bastion start-chunk origin"),
    QT_TRANSLATE_NOOP("LootEditor", "Relative to Location reference (X/Z)"),
    QT_TRANSLATE_NOOP("LootEditor",
        "Relative X/Z use the bastion start-chunk origin. Relative Y uses "
        "Y=32 as its origin. Range endpoints are inclusive."),
    QT_TRANSLATE_NOOP("LootEditor",
        "X/Z are relative to the result selected by this condition's "
        "'Location is relative to' setting. Y remains an absolute world "
        "coordinate. With no reference, X/Z are relative to the search origin."),
    QT_TRANSLATE_NOOP("LootEditor",
        "Village pieces and Loot-container coordinates include surface-height "
        "calculation. Candidates whose content RNG cannot be determined safely "
        "are treated as unknown and are not mixed into final results. "
        "Position-only searches remain available. Count ranges are inclusive."),
    QT_TRANSLATE_NOOP("LootEditor",
        "Count ranges are inclusive and may have no maximum. Selecting an "
        "enchanted book also enables enchantment and level filters."),
    QT_TRANSLATE_NOOP("LootEditor",
        "Bastion layout and chest Loot are reconstructed exactly from the "
        "lower 48 bits in Java 1.16. Select an enchantable book or piece of "
        "equipment to enable enchantment and level filters."),
};

QString itemDisplayName(int item)
{
    static const char *english[DP_LOOT_ITEM_COUNT] = {
        QT_TRANSLATE_NOOP("LootItem", "Diamond"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Ingot"),
        QT_TRANSLATE_NOOP("LootItem", "Gold Ingot"),
        QT_TRANSLATE_NOOP("LootItem", "Emerald"),
        QT_TRANSLATE_NOOP("LootItem", "Bone"),
        QT_TRANSLATE_NOOP("LootItem", "Spider Eye"),
        QT_TRANSLATE_NOOP("LootItem", "Rotten Flesh"),
        QT_TRANSLATE_NOOP("LootItem", "Saddle"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Horse Armor"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Horse Armor"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Horse Armor"),
        QT_TRANSLATE_NOOP("LootItem", "Enchanted Book"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Apple"),
        QT_TRANSLATE_NOOP("LootItem", "Enchanted Golden Apple"),
        QT_TRANSLATE_NOOP("LootItem", "Gunpowder"),
        QT_TRANSLATE_NOOP("LootItem", "String"),
        QT_TRANSLATE_NOOP("LootItem", "Sand"),
        QT_TRANSLATE_NOOP("LootItem", "Heart of the Sea"),
        QT_TRANSLATE_NOOP("LootItem", "TNT"),
        QT_TRANSLATE_NOOP("LootItem", "Prismarine Crystals"),
        QT_TRANSLATE_NOOP("LootItem", "Leather Chestplate"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Sword"),
        QT_TRANSLATE_NOOP("LootItem", "Cooked Cod"),
        QT_TRANSLATE_NOOP("LootItem", "Cooked Salmon"),
        QT_TRANSLATE_NOOP("LootItem", "Obsidian"),
        QT_TRANSLATE_NOOP("LootItem", "Flint"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Nugget"),
        QT_TRANSLATE_NOOP("LootItem", "Flint and Steel"),
        QT_TRANSLATE_NOOP("LootItem", "Fire Charge"),
        QT_TRANSLATE_NOOP("LootItem", "Gold Nugget"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Sword"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Axe"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Hoe"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Shovel"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Pickaxe"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Boots"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Chestplate"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Helmet"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Leggings"),
        QT_TRANSLATE_NOOP("LootItem", "Glistering Melon Slice"),
        QT_TRANSLATE_NOOP("LootItem", "Light Weighted Pressure Plate"),
        QT_TRANSLATE_NOOP("LootItem", "Golden Carrot"),
        QT_TRANSLATE_NOOP("LootItem", "Clock"),
        QT_TRANSLATE_NOOP("LootItem", "Block of Gold"),
        QT_TRANSLATE_NOOP("LootItem", "Bell"),
        QT_TRANSLATE_NOOP("LootItem", "Treasure Map"),
        QT_TRANSLATE_NOOP("LootItem", "Compass"),
        QT_TRANSLATE_NOOP("LootItem", "Empty Map"),
        QT_TRANSLATE_NOOP("LootItem", "Paper"),
        QT_TRANSLATE_NOOP("LootItem", "Feather"),
        QT_TRANSLATE_NOOP("LootItem", "Book"),
        QT_TRANSLATE_NOOP("LootItem", "Potato"),
        QT_TRANSLATE_NOOP("LootItem", "Poisonous Potato"),
        QT_TRANSLATE_NOOP("LootItem", "Carrot"),
        QT_TRANSLATE_NOOP("LootItem", "Wheat"),
        QT_TRANSLATE_NOOP("LootItem", "Suspicious Stew"),
        QT_TRANSLATE_NOOP("LootItem", "Coal"),
        QT_TRANSLATE_NOOP("LootItem", "Pumpkin"),
        QT_TRANSLATE_NOOP("LootItem", "Bamboo"),
        QT_TRANSLATE_NOOP("LootItem", "Leather Cap"),
        QT_TRANSLATE_NOOP("LootItem", "Leather Pants"),
        QT_TRANSLATE_NOOP("LootItem", "Leather Boots"),
        QT_TRANSLATE_NOOP("LootItem", "Bottle o' Enchanting"),
        QT_TRANSLATE_NOOP("LootItem", "Lapis Lazuli"),
        QT_TRANSLATE_NOOP("LootItem", "Bread"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Helmet"),
        QT_TRANSLATE_NOOP("LootItem", "Raw Porkchop"),
        QT_TRANSLATE_NOOP("LootItem", "Raw Beef"),
        QT_TRANSLATE_NOOP("LootItem", "Raw Mutton"),
        QT_TRANSLATE_NOOP("LootItem", "Stick"),
        QT_TRANSLATE_NOOP("LootItem", "Clay Ball"),
        QT_TRANSLATE_NOOP("LootItem", "Green Dye"),
        QT_TRANSLATE_NOOP("LootItem", "Cactus"),
        QT_TRANSLATE_NOOP("LootItem", "Raw Cod"),
        QT_TRANSLATE_NOOP("LootItem", "Raw Salmon"),
        QT_TRANSLATE_NOOP("LootItem", "Water Bucket"),
        QT_TRANSLATE_NOOP("LootItem", "Barrel"),
        QT_TRANSLATE_NOOP("LootItem", "Wheat Seeds"),
        QT_TRANSLATE_NOOP("LootItem", "Arrow"),
        QT_TRANSLATE_NOOP("LootItem", "Egg"),
        QT_TRANSLATE_NOOP("LootItem", "Flower Pot"),
        QT_TRANSLATE_NOOP("LootItem", "Stone"),
        QT_TRANSLATE_NOOP("LootItem", "Stone Bricks"),
        QT_TRANSLATE_NOOP("LootItem", "Yellow Dye"),
        QT_TRANSLATE_NOOP("LootItem", "Smooth Stone"),
        QT_TRANSLATE_NOOP("LootItem", "Dandelion"),
        QT_TRANSLATE_NOOP("LootItem", "Poppy"),
        QT_TRANSLATE_NOOP("LootItem", "Apple"),
        QT_TRANSLATE_NOOP("LootItem", "Oak Sapling"),
        QT_TRANSLATE_NOOP("LootItem", "Grass"),
        QT_TRANSLATE_NOOP("LootItem", "Tall Grass"),
        QT_TRANSLATE_NOOP("LootItem", "Acacia Sapling"),
        QT_TRANSLATE_NOOP("LootItem", "Torch"),
        QT_TRANSLATE_NOOP("LootItem", "Bucket"),
        QT_TRANSLATE_NOOP("LootItem", "White Wool"),
        QT_TRANSLATE_NOOP("LootItem", "Black Wool"),
        QT_TRANSLATE_NOOP("LootItem", "Gray Wool"),
        QT_TRANSLATE_NOOP("LootItem", "Brown Wool"),
        QT_TRANSLATE_NOOP("LootItem", "Light Gray Wool"),
        QT_TRANSLATE_NOOP("LootItem", "Shears"),
        QT_TRANSLATE_NOOP("LootItem", "Blue Ice"),
        QT_TRANSLATE_NOOP("LootItem", "Snow Block"),
        QT_TRANSLATE_NOOP("LootItem", "Beetroot Seeds"),
        QT_TRANSLATE_NOOP("LootItem", "Beetroot Soup"),
        QT_TRANSLATE_NOOP("LootItem", "Furnace"),
        QT_TRANSLATE_NOOP("LootItem", "Snowball"),
        QT_TRANSLATE_NOOP("LootItem", "Fern"),
        QT_TRANSLATE_NOOP("LootItem", "Large Fern"),
        QT_TRANSLATE_NOOP("LootItem", "Sweet Berries"),
        QT_TRANSLATE_NOOP("LootItem", "Pumpkin Seeds"),
        QT_TRANSLATE_NOOP("LootItem", "Pumpkin Pie"),
        QT_TRANSLATE_NOOP("LootItem", "Spruce Sapling"),
        QT_TRANSLATE_NOOP("LootItem", "Spruce Sign"),
        QT_TRANSLATE_NOOP("LootItem", "Spruce Log"),
        QT_TRANSLATE_NOOP("LootItem", "Leather"),
        QT_TRANSLATE_NOOP("LootItem", "Redstone Dust"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Pickaxe"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Shovel"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Chestplate"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Leggings"),
        QT_TRANSLATE_NOOP("LootItem", "Iron Boots"),
        QT_TRANSLATE_NOOP("LootItem", "Lodestone"),
        QT_TRANSLATE_NOOP("LootItem", "Crossbow"),
        QT_TRANSLATE_NOOP("LootItem", "Spectral Arrow"),
        QT_TRANSLATE_NOOP("LootItem", "Gilded Blackstone"),
        QT_TRANSLATE_NOOP("LootItem", "Crying Obsidian"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Shovel"),
        QT_TRANSLATE_NOOP("LootItem", "Netherite Scrap"),
        QT_TRANSLATE_NOOP("LootItem", "Ancient Debris"),
        QT_TRANSLATE_NOOP("LootItem", "Glowstone"),
        QT_TRANSLATE_NOOP("LootItem", "Soul Sand"),
        QT_TRANSLATE_NOOP("LootItem", "Crimson Nylium"),
        QT_TRANSLATE_NOOP("LootItem", "Cooked Porkchop"),
        QT_TRANSLATE_NOOP("LootItem", "Crimson Fungus"),
        QT_TRANSLATE_NOOP("LootItem", "Crimson Roots"),
        QT_TRANSLATE_NOOP("LootItem", "Snout Banner Pattern"),
        QT_TRANSLATE_NOOP("LootItem", "Music Disc (Pigstep)"),
        QT_TRANSLATE_NOOP("LootItem", "Chain"),
        QT_TRANSLATE_NOOP("LootItem", "Magma Cream"),
        QT_TRANSLATE_NOOP("LootItem", "Bone Block"),
        QT_TRANSLATE_NOOP("LootItem", "Netherite Ingot"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Sword"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Chestplate"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Helmet"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Leggings"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Boots"),
        QT_TRANSLATE_NOOP("LootItem", "Nether Quartz"),
        QT_TRANSLATE_NOOP("LootItem", "Dead Bush"),
        QT_TRANSLATE_NOOP("LootItem", "Any item (Loot container position/count only)"),
        QT_TRANSLATE_NOOP("LootItem", "Diamond Pickaxe"),
        QT_TRANSLATE_NOOP("LootItem", "Block of Iron"),
    };
    return QCoreApplication::translate("LootItem", english[item]) + " (" +
        QString::fromLatin1(structureLootItemName(item)) + ")";
}

QString enchantmentDisplayName(int enchantment)
{
    static const char *english[DP_ENCH_COUNT] = {
        QT_TRANSLATE_NOOP("LootEnchantment", "Protection"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Fire Protection"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Feather Falling"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Blast Protection"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Projectile Protection"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Respiration"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Aqua Affinity"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Thorns"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Depth Strider"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Frost Walker"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Curse of Binding"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Sharpness"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Smite"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Bane of Arthropods"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Knockback"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Fire Aspect"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Looting"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Sweeping Edge"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Efficiency"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Silk Touch"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Unbreaking"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Fortune"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Power"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Punch"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Flame"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Infinity"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Luck of the Sea"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Lure"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Loyalty"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Impaling"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Riptide"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Channeling"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Multishot"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Quick Charge"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Piercing"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Mending"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Curse of Vanishing"),
        QT_TRANSLATE_NOOP("LootEnchantment", "Soul Speed"),
    };
    return QCoreApplication::translate(
        "LootEnchantment", english[enchantment]) + " (" +
        QString::fromLatin1(
            desertPyramidEnchantmentName(enchantment)) + ")";
}

QString lootTr(const char *source)
{
    Q_UNUSED(lootEditorTranslationKeys);
    return QCoreApplication::translate("LootEditor", source);
}

}

class LootRuleRow : public QWidget
{
public:
    enum ItemDataRole {
        EnchantedOnlyRole = Qt::UserRole + 1,
    };

    explicit LootRuleRow(
        int structureType, int bastionLootProfile,
        QWidget *parent = nullptr)
        : QWidget(parent)
        , m_structureType(structureType)
        , m_bastionLootProfile(bastionLootProfile)
    {
        QGridLayout *layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        item = new QComboBox(this);
        updateItems();
        item->setMinimumContentsLength(16);

        minCount = new QSpinBox(this);
        minCount->setRange(0, 2000000000);
        minCount->setValue(1);
        maxCount = new QSpinBox(this);
        maxCount->setRange(-1, 2000000000);
        maxCount->setSpecialValueText(lootTr("No maximum"));
        maxCount->setValue(-1);

        enchantment = new QComboBox(this);
        updateEnchantments();

        minLevel = new QSpinBox(this);
        maxLevel = new QSpinBox(this);
        minLevel->setRange(1, DP_ENCH_MAX_LEVEL);
        maxLevel->setRange(1, DP_ENCH_MAX_LEVEL);
        minLevel->setValue(1);
        maxLevel->setValue(DP_ENCH_MAX_LEVEL);

        remove = new QPushButton(lootTr("Remove"), this);

        layout->addWidget(item, 0, 0, 1, 3);
        layout->addWidget(
            new QLabel(lootTr("Count"), this), 0, 3);
        layout->addWidget(minCount, 0, 4);
        layout->addWidget(new QLabel("–", this), 0, 5);
        layout->addWidget(maxCount, 0, 6);
        layout->addWidget(remove, 0, 7);

        layout->addWidget(
            new QLabel(lootTr("Enchantment"), this), 1, 0);
        layout->addWidget(enchantment, 1, 1, 1, 3);
        layout->addWidget(
            new QLabel(lootTr("Level"), this), 1, 4);
        layout->addWidget(minLevel, 1, 5);
        layout->addWidget(new QLabel("–", this), 1, 6);
        layout->addWidget(maxLevel, 1, 7);
        layout->setColumnStretch(2, 1);

        connect(item, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this] {
                    updateEnchantments();
                    updateEnchantmentState();
                });
        connect(enchantment, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this] { updateEnchantmentState(); });
        updateEnchantmentState();
    }

    LootRule value() const
    {
        LootRule rule;
        rule.item = item->currentData().toInt();
        rule.minCount = minCount->value();
        rule.maxCount = maxCount->value();
        rule.enchantment = enchantment->currentData().toInt();
        rule.minLevel = minLevel->value();
        rule.maxLevel = maxLevel->value();
        return rule;
    }

    void setValue(const LootRule& rule)
    {
        int itemIndex = -1;
        const bool enchantedOnly =
            rule.enchantment != LootRule::ENCHANTMENT_NONE;
        for (int index = 0; index < item->count(); index++)
        {
            if (item->itemData(index).toInt() == rule.item &&
                item->itemData(index, EnchantedOnlyRole).toBool() ==
                    enchantedOnly)
            {
                itemIndex = index;
                break;
            }
        }
        if (itemIndex < 0)
            itemIndex = item->findData(rule.item);
        item->setCurrentIndex(itemIndex);
        updateEnchantments(rule.enchantment);
        minCount->setValue(rule.minCount);
        maxCount->setValue(rule.maxCount);
        minLevel->setValue(rule.minLevel);
        maxLevel->setValue(rule.maxLevel);
        updateEnchantmentState();
    }

    void setContext(int structureType, int bastionLootProfile)
    {
        if (m_structureType == structureType &&
            m_bastionLootProfile == bastionLootProfile)
            return;
        m_structureType = structureType;
        m_bastionLootProfile = bastionLootProfile;
        updateItems();
        updateEnchantments();
        updateEnchantmentState();
    }

    void updateItems()
    {
        int oldItem = item->currentData().toInt();
        bool oldEnchantedOnly =
            item->currentData(EnchantedOnlyRole).toBool();
        item->clear();
        for (int i = 0; i < DP_LOOT_ITEM_COUNT; i++)
        {
            if (structureLootItemAvailable(
                    m_structureType, i, m_bastionLootProfile))
            {
                item->addItem(itemDisplayName(i), i);
                if ((m_structureType == Bastion || m_bastionLootProfile == LOOT_PROFILE_26_2) &&
                    i != DP_LOOT_ENCHANTED_BOOK &&
                    structureLootItemCanBeEnchanted(
                        m_structureType, i,
                        m_bastionLootProfile))
                {
                    item->addItem(
                        lootTr("Enchanted %1").arg(
                            itemDisplayName(i)), i);
                    item->setItemData(
                        item->count() - 1, true,
                        EnchantedOnlyRole);
                }
            }
        }
        int index = -1;
        for (int candidate = 0;
             candidate < item->count(); candidate++)
        {
            if (item->itemData(candidate).toInt() == oldItem &&
                item->itemData(
                    candidate, EnchantedOnlyRole).toBool() ==
                    oldEnchantedOnly)
            {
                index = candidate;
                break;
            }
        }
        item->setCurrentIndex(index >= 0 ? index : 0);
    }

    void updateEnchantmentState()
    {
        const bool enchantable = enchantment->count() > 1;
        enchantment->setEnabled(enchantable);
        int ench = enchantment->currentData().toInt();
        bool levels = enchantable &&
            ench != LootRule::ENCHANTMENT_NONE;
        minLevel->setEnabled(levels);
        maxLevel->setEnabled(levels);
        int maximum = levels && ench >= 0
            ? desertPyramidEnchantmentMaxLevel(ench)
            : DP_ENCH_MAX_LEVEL;
        minLevel->setMaximum(maximum);
        maxLevel->setMaximum(maximum);
        if (maxLevel->value() < minLevel->value())
            maxLevel->setValue(minLevel->value());
    }

    void updateEnchantments(int requested = -2)
    {
        int oldEnchantment = requested == -2
            ? enchantment->currentData().toInt()
            : requested;
        const int selectedItem = item->currentData().toInt();
        const bool enchantedOnly =
            item->currentData(EnchantedOnlyRole).toBool();
        enchantment->clear();
        if (!enchantedOnly)
        {
            enchantment->addItem(
                lootTr("No enchantment filter"),
                LootRule::ENCHANTMENT_NONE);
        }
        const bool canBeEnchanted =
            structureLootItemCanBeEnchanted(
                m_structureType, selectedItem,
                m_bastionLootProfile);
        if (canBeEnchanted)
        {
            enchantment->addItem(
                lootTr("Any enchantment (enchanted items only)"),
                LootRule::ENCHANTMENT_ANY);
        }
        for (int i = 0; i < DP_ENCH_COUNT; i++)
        {
            const bool available =
                selectedItem == DP_LOOT_ENCHANTED_BOOK
                    ? ((m_structureType != Bastion && i < DP_ENCH_SOUL_SPEED) ||
                       structureLootEnchantmentAvailable(
                           m_structureType, selectedItem, i,
                           m_bastionLootProfile))
                    : structureLootEnchantmentAvailable(
                          m_structureType, selectedItem, i,
                          m_bastionLootProfile);
            if (available)
                enchantment->addItem(enchantmentDisplayName(i), i);
        }
        int index = enchantment->findData(oldEnchantment);
        if (index < 0 && enchantedOnly)
            index = enchantment->findData(
                LootRule::ENCHANTMENT_ANY);
        enchantment->setCurrentIndex(index >= 0 ? index : 0);
    }

    QComboBox *item;
    QSpinBox *minCount;
    QSpinBox *maxCount;
    QComboBox *enchantment;
    QSpinBox *minLevel;
    QSpinBox *maxLevel;
    QPushButton *remove;
    int m_structureType;
    int m_bastionLootProfile;
};

LootRuleEditor::LootRuleEditor(QWidget *parent)
    : QGroupBox(lootTr("Chest content conditions"), parent)
    , m_structureType(Desert_Pyramid)
    , m_mc(MC_1_16_1)
    , m_areaTotal(false)
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    m_enabled = new QCheckBox(lootTr("Filter by chest contents"), this);
    outer->addWidget(m_enabled);

    QFormLayout *options = new QFormLayout();
    m_logic = new QComboBox(this);
    m_logic->addItem(
        lootTr("Match all (AND)"),
        LootRuleSet::LOGIC_ALL);
    m_logic->addItem(
        lootTr("Match any (OR)"),
        LootRuleSet::LOGIC_ANY);
    options->addRow(lootTr("Item conditions"), m_logic);

    m_instanceMode = new QComboBox(this);
    m_instanceMode->addItem(
        lootTr("Any structure matches"),
        LootRuleSet::INSTANCE_ANY);
    m_instanceMode->addItem(
        lootTr("Every structure matches"),
        LootRuleSet::INSTANCE_EVERY);
    m_instanceMode->addItem(
        lootTr("Total across structures in range"),
        LootRuleSet::INSTANCE_TOTAL);
    options->addRow(lootTr("Multiple structures"), m_instanceMode);

    m_chestMode = new QComboBox(this);
    updateChestModes();
    options->addRow(lootTr("Chests within structure"), m_chestMode);

    m_bastionLootProfileLabel =
        new QLabel(lootTr("Bastion Loot version"), this);
    m_bastionLootProfile = new QComboBox(this);
    m_bastionLootProfile->addItem(
        lootTr("Java 1.16–1.16.1 (original Loot)"),
        LootRuleSet::BASTION_LOOT_1_16_1);
    m_bastionLootProfile->addItem(
        lootTr("Java 1.16.2–1.16.5 (tweaked Loot)"),
        LootRuleSet::BASTION_LOOT_1_16_2_TO_1_16_5);
    m_bastionLootProfile->setCurrentIndex(
        m_bastionLootProfile->findData(
            LootRuleSet::BASTION_LOOT_1_16_2_TO_1_16_5));
    const QString bastionProfileHelp = lootTr(
        "Bastion chest tables changed in Java 1.16.2. The tweaked profile "
        "includes enchanted diamond pickaxes in Hoglin Stable and Generic "
        "chests. The four chest-table categories remain separate.");
    m_bastionLootProfileLabel->setToolTip(bastionProfileHelp);
    m_bastionLootProfile->setToolTip(bastionProfileHelp);
    options->addRow(
        m_bastionLootProfileLabel, m_bastionLootProfile);
    outer->addLayout(options);

    m_positionPanel = new QWidget(this);
    QVBoxLayout *positionOuter = new QVBoxLayout(m_positionPanel);
    positionOuter->setContentsMargins(0, 0, 0, 0);
    QFormLayout *positionOptions = new QFormLayout();
    m_positionMode = new QComboBox(m_positionPanel);
    updatePositionModes();
    positionOptions->addRow(
        lootTr("Chest coordinates"), m_positionMode);
    positionOuter->addLayout(positionOptions);

    m_positionRange = new QWidget(m_positionPanel);
    QGridLayout *positionGrid = new QGridLayout(m_positionRange);
    positionGrid->setContentsMargins(0, 0, 0, 0);
    auto setupCoordinate = [](QSpinBox *box, int value)
    {
        box->setRange(-30000000, 30000000);
        box->setValue(value);
    };
    m_chestMinX = new QSpinBox(m_positionRange);
    m_chestMaxX = new QSpinBox(m_positionRange);
    m_chestMinY = new QSpinBox(m_positionRange);
    m_chestMaxY = new QSpinBox(m_positionRange);
    m_chestMinZ = new QSpinBox(m_positionRange);
    m_chestMaxZ = new QSpinBox(m_positionRange);
    setupCoordinate(m_chestMinX, -30000000);
    setupCoordinate(m_chestMaxX, 30000000);
    setupCoordinate(m_chestMinY, -64);
    setupCoordinate(m_chestMaxY, 320);
    setupCoordinate(m_chestMinZ, -30000000);
    setupCoordinate(m_chestMaxZ, 30000000);
    QSpinBox *minimums[] = {
        m_chestMinX, m_chestMinY, m_chestMinZ,
    };
    QSpinBox *maximums[] = {
        m_chestMaxX, m_chestMaxY, m_chestMaxZ,
    };
    const char *axisNames[] = {"X", "Y", "Z"};
    for (int axis = 0; axis < 3; axis++)
    {
        positionGrid->addWidget(
            new QLabel(QString::fromLatin1(axisNames[axis]),
                       m_positionRange),
            axis, 0);
        positionGrid->addWidget(minimums[axis], axis, 1);
        positionGrid->addWidget(new QLabel("–", m_positionRange),
                                axis, 2);
        positionGrid->addWidget(maximums[axis], axis, 3);
    }
    positionOuter->addWidget(m_positionRange);
    m_positionHint = new QLabel(m_positionPanel);
    m_positionHint->setWordWrap(true);
    positionOuter->addWidget(m_positionHint);
    outer->addWidget(m_positionPanel);

    m_support = new QLabel(this);
    m_support->setWordWrap(true);
    outer->addWidget(m_support);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setMinimumHeight(100);
    scroll->setMaximumHeight(260);
    m_rowsWidget = new QWidget(scroll);
    m_rowsLayout = new QVBoxLayout(m_rowsWidget);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->addStretch();
    scroll->setWidget(m_rowsWidget);
    outer->addWidget(scroll);

    m_add = new QPushButton(lootTr("+ Add item condition"), this);
    outer->addWidget(m_add, 0, Qt::AlignLeft);

    connect(m_enabled, &QCheckBox::toggled,
            this, [this] { updateEnabledState(); });
    connect(m_instanceMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { updateEnabledState(); });
    connect(m_positionMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] { updatePositionState(); });
    connect(m_bastionLootProfile,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] {
                const int profile =
                    lootProfileForVersion(m_mc, m_bastionLootProfile->currentData().toInt());
                for (LootRuleRow *row : m_rows)
                    row->setContext(m_structureType, profile);
                updateEnabledState();
            });
    connect(m_add, &QPushButton::clicked,
            this, [this] { addRule(); });
    addRule();
    m_enabled->setChecked(false);
    updateEnabledState();
}

void LootRuleEditor::setContext(int structureType, int mc)
{
    bool changed = m_structureType != structureType || m_mc != mc;
    m_structureType = structureType;
    m_mc = mc;
    if (changed)
    {
        const int profile =
            lootProfileForVersion(m_mc, m_bastionLootProfile->currentData().toInt());
        for (LootRuleRow *row : m_rows)
            row->setContext(structureType, profile);
        updateChestModes();
        updatePositionModes();
    }
    updateEnabledState();
}

void LootRuleEditor::setAreaTotalMode(bool areaTotal)
{
    m_areaTotal = areaTotal;
    if (areaTotal)
    {
        m_enabled->setChecked(true);
        m_instanceMode->setCurrentIndex(
            m_instanceMode->findData(LootRuleSet::INSTANCE_TOTAL));
    }
    updateEnabledState();
}

void LootRuleEditor::setRuleSet(
    const LootRuleSet& rules, bool enabled)
{
    setContext(rules.structureType, m_mc);
    while (!m_rows.isEmpty())
        removeRule(m_rows.last());
    m_logic->setCurrentIndex(m_logic->findData(rules.logic));
    m_instanceMode->setCurrentIndex(
        m_instanceMode->findData(rules.instanceMode));
    m_chestMode->setCurrentIndex(
        m_chestMode->findData(rules.chestMode));
    m_bastionLootProfile->setCurrentIndex(
        m_bastionLootProfile->findData(
            rules.bastionLootProfile));
    m_positionMode->setCurrentIndex(
        m_positionMode->findData(rules.chestPositionMode));
    m_chestMinX->setValue(rules.chestMinX);
    m_chestMaxX->setValue(rules.chestMaxX);
    m_chestMinY->setValue(rules.chestMinY);
    m_chestMaxY->setValue(rules.chestMaxY);
    m_chestMinZ->setValue(rules.chestMinZ);
    m_chestMaxZ->setValue(rules.chestMaxZ);
    for (const LootRule& rule : rules.rules)
        addRule(rule);
    if (m_rows.isEmpty())
        addRule();
    m_enabled->setChecked(enabled || m_areaTotal);
    updateEnabledState();
}

bool LootRuleEditor::lootEnabled() const
{
    return m_enabled->isChecked() && isLootSupported(
        m_structureType, m_mc);
}

LootRuleSet LootRuleEditor::ruleSet() const
{
    LootRuleSet rules;
    rules.structureType = m_structureType;
    rules.logic = m_logic->currentData().toInt();
    rules.instanceMode = m_areaTotal
        ? LootRuleSet::INSTANCE_TOTAL
        : m_instanceMode->currentData().toInt();
    rules.chestMode = m_chestMode->currentData().toInt();
    rules.bastionLootProfile =
        m_bastionLootProfile->currentData().toInt();
    rules.chestPositionMode =
        m_positionMode->currentData().toInt();
    rules.chestMinX = m_chestMinX->value();
    rules.chestMaxX = m_chestMaxX->value();
    rules.chestMinY = m_chestMinY->value();
    rules.chestMaxY = m_chestMaxY->value();
    rules.chestMinZ = m_chestMinZ->value();
    rules.chestMaxZ = m_chestMaxZ->value();
    for (LootRuleRow *row : m_rows)
        rules.rules.push_back(row->value());
    return rules;
}

void LootRuleEditor::addRule(const LootRule& rule)
{
    LootRuleRow *row =
        new LootRuleRow(
            m_structureType,
            lootProfileForVersion(m_mc, m_bastionLootProfile->currentData().toInt()),
            m_rowsWidget);
    row->setValue(rule);
    m_rows.insert(m_rows.size(), row);
    m_rowsLayout->insertWidget(m_rowsLayout->count() - 1, row);
    connect(row->remove, &QPushButton::clicked,
            this, [this, row] { removeRule(row); });
}

void LootRuleEditor::removeRule(LootRuleRow *row)
{
    m_rows.removeOne(row);
    m_rowsLayout->removeWidget(row);
    delete row;
}

void LootRuleEditor::updateChestModes()
{
    int oldMode = m_chestMode->currentData().toInt();
    m_chestMode->clear();
    m_chestMode->addItem(
        lootTr("Total across all chests"),
        LootRuleSet::CHESTS_TOTAL);
    m_chestMode->addItem(
        lootTr("Any single chest matches"),
        LootRuleSet::CHEST_ANY);
    m_chestMode->addItem(
        lootTr("Every chest matches"),
        LootRuleSet::CHEST_EVERY);

    if (m_structureType == Desert_Pyramid)
    {
        for (int i = 0; i < 4; i++)
        {
            m_chestMode->addItem(
                lootTr("Generation-order chest %1").arg(i + 1),
                LootRuleSet::CHEST_1 + i);
        }
    }
    else if (m_structureType == Shipwreck)
    {
        m_chestMode->addItem(
            lootTr("Supply chest"),
            LootRuleSet::CHEST_1);
        m_chestMode->addItem(
            lootTr("Map chest"),
            LootRuleSet::CHEST_2);
        m_chestMode->addItem(
            lootTr("Treasure chest"),
            LootRuleSet::CHEST_3);
    }
    else if (m_structureType != Bastion &&
             m_structureType != Village)
    {
        m_chestMode->addItem(
            lootTr("Only chest"),
            LootRuleSet::CHEST_1);
    }
    int index = m_chestMode->findData(oldMode);
    m_chestMode->setCurrentIndex(index >= 0 ? index : 0);
}

void LootRuleEditor::updatePositionModes()
{
    int oldMode = m_positionMode->currentData().toInt();
    m_positionMode->clear();
    m_positionMode->addItem(
        lootTr("Any position"),
        LootRuleSet::CHEST_POSITION_ANY);
    m_positionMode->addItem(
        lootTr("Absolute world coordinates"),
        LootRuleSet::CHEST_POSITION_ABSOLUTE);
    if (m_structureType == Bastion)
    {
        m_positionMode->addItem(
            lootTr("Relative to bastion start-chunk origin"),
            LootRuleSet::CHEST_POSITION_RELATIVE);
    }
    if (m_structureType == Bastion ||
        m_structureType == Village)
    {
        m_positionMode->addItem(
            lootTr("Relative to Location reference (X/Z)"),
            LootRuleSet::CHEST_POSITION_LOCATION_REFERENCE);
    }
    int index = m_positionMode->findData(oldMode);
    m_positionMode->setCurrentIndex(index >= 0 ? index : 0);
}

void LootRuleEditor::updatePositionState()
{
    const int mode = m_positionMode->currentData().toInt();
    m_positionRange->setVisible(
        mode != LootRuleSet::CHEST_POSITION_ANY);
    m_positionHint->setVisible(
        mode == LootRuleSet::CHEST_POSITION_RELATIVE ||
        mode ==
            LootRuleSet::CHEST_POSITION_LOCATION_REFERENCE);
    if (mode == LootRuleSet::CHEST_POSITION_RELATIVE)
    {
        m_positionHint->setText(lootTr(
            "Relative X/Z use the bastion start-chunk origin. Relative Y uses "
            "Y=32 as its origin. Range endpoints are inclusive."));
    }
    else if (mode ==
             LootRuleSet::CHEST_POSITION_LOCATION_REFERENCE)
    {
        m_positionHint->setText(lootTr(
            "X/Z are relative to the result selected by this condition's "
            "'Location is relative to' setting. Y remains an absolute world "
            "coordinate. With no reference, X/Z are relative to the search origin."));
    }
    else
    {
        m_positionHint->clear();
    }
}

void LootRuleEditor::updateEnabledState()
{
    QString unsupported =
        lootSupportDescription(m_structureType, m_mc);
    bool supported = unsupported.isEmpty();
    if (!supported)
    {
        m_support->setText(unsupported);
        m_support->show();
        setToolTip(QString());
    }
    else if (lootUsesFullSeed(m_structureType, m_mc))
    {
        const QString help = lootTr(
            "Java 26.2 chest tables and enchantments. Chest contents use all 64 seed bits; "
            "48-bit-only searches retain candidates without deciding Loot. Bastion layouts "
            "are cached per lower-48 family, but each full seed gets its own chest contents. "
            "Portal biome/terrain viability uses the viewer's existing approximation; "
            "terrain-dependent missing or overlapping portals are not resolved.");
        m_support->hide();
        setToolTip(help);
        m_enabled->setToolTip(help);
    }
    else if (m_structureType == Village)
    {
        const QString help = lootTr(
            "Village pieces and Loot-container coordinates include surface-height "
            "calculation. Candidates whose content RNG cannot be determined safely "
            "are treated as unknown and are not mixed into final results. "
            "Position-only searches remain available. Count ranges are inclusive.");
        m_support->hide();
        setToolTip(help);
        m_enabled->setToolTip(help);
    }
    else if (m_structureType == Bastion)
    {
        const QString help = lootTr(
            "Bastion layout and chest Loot are reconstructed exactly from the "
            "lower 48 bits in Java 1.16. Select an enchantable book or piece of "
            "equipment to enable enchantment and level filters.");
        m_support->hide();
        setToolTip(help);
        m_enabled->setToolTip(help);
    }
    else
    {
        const QString help = lootTr(
            "Count ranges are inclusive and may have no maximum. Selecting an "
            "enchanted book also enables enchantment and level filters.");
        m_support->hide();
        setToolTip(help);
        m_enabled->setToolTip(help);
    }

    m_enabled->setVisible(!m_areaTotal);
    if (m_areaTotal)
        m_enabled->setChecked(true);
    bool active = supported && (m_areaTotal || m_enabled->isChecked());
    m_logic->setEnabled(active);
    m_instanceMode->setEnabled(active && !m_areaTotal);
    bool total = m_areaTotal ||
        m_instanceMode->currentData().toInt() ==
            LootRuleSet::INSTANCE_TOTAL;
    if (total)
    {
        m_chestMode->setCurrentIndex(
            m_chestMode->findData(LootRuleSet::CHESTS_TOTAL));
    }
    m_chestMode->setEnabled(active && !total);
    const bool isBastion = m_structureType == Bastion && m_mc != MC_26_2;
    m_bastionLootProfileLabel->setVisible(isBastion);
    m_bastionLootProfile->setVisible(isBastion);
    m_bastionLootProfile->setEnabled(active && isBastion);
    const bool supportsPosition =
        m_structureType == Bastion || m_structureType == Village;
    m_positionPanel->setVisible(supportsPosition);
    m_positionMode->setEnabled(active && supportsPosition);
    m_positionRange->setEnabled(
        active &&
        m_positionMode->currentData().toInt() !=
            LootRuleSet::CHEST_POSITION_ANY);
    updatePositionState();
    m_rowsWidget->setEnabled(active);
    m_add->setEnabled(active);
}
