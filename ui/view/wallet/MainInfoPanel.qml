import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import Beam.Wallet 1.0
import "../controls"

Control {
    id: control

    InfoViewModel {
        id: viewModel
    }

    property alias selectedAsset: viewModel.selectedAsset
    property bool  showProgress:  viewModel.progress.length > 0

    spacing: 10
    implicitHeight: control.showProgress ? 200 : 130

    property real itemWidth: {
        return showProgress ? (control.availableWidth - control.spacing) / 2 : control.availableWidth
    }

    contentItem: Row {
        spacing: 10

        AvailablePanelNew {
            id:         avctrl
            width:      control.itemWidth
            height:     control.availableHeight
            compact:    control.showProgress
            available:  viewModel.assetAvailable
            rateUnit:   viewModel.rateUnit
            rate:       viewModel.rate
            icon:       viewModel.assetIcon
            unitName:   viewModel.assetUnitName
            assetName:  viewModel.assetName
        }

        InProgressPanelNew {
            id:        ipctrl
            width:     control.itemWidth
            height:    control.availableHeight
            visible:   showProgress
            progress:  viewModel.progress
            totals:    viewModel.progressTotal
        }
    }
}
